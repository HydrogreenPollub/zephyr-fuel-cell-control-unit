#include "fccu.h"
#include "fccu_analog.h"
#include "fccu_fan.h"
#include "fccu_digital.h"
#include "fccu_flow.h"
#include "fccu_log.h"
#include "can.h"
#include "candef.h"
#include "counter.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fccu, LOG_LEVEL_INF);


volatile fccu_flags_t flags;
volatile fccu_state_t state = STOPPED;

static uint32_t purge_periodic_ticks;

fccu_can_t can = {
    .can_device     = DEVICE_DT_GET(DT_ALIAS(can)),
    .can_status_led = GPIO_DT_SPEC_GET(DT_ALIAS(can_status_led), gpios),
};

static fccu_counter_t counter = {
    .counter_measurements            = DEVICE_DT_GET(DT_ALIAS(counter0)),
    .counter_fuel_cell_voltage_check = DEVICE_DT_GET(DT_ALIAS(counter1)),
    .counter2                        = DEVICE_DT_GET(DT_ALIAS(counter2)),
    .counter3                        = DEVICE_DT_GET(DT_ALIAS(counter3)),
};

static void can_state_change_cb(const struct device *dev, enum can_state cs,
                                struct can_bus_err_cnt err_cnt, void *user_data);
static void can_led_work_fn(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(can_led_work, can_led_work_fn);

static void can_led_work_fn(struct k_work *work)
{
    enum can_state cs;
    struct can_bus_err_cnt ec;
    if (can_get_state(can.can_device, &cs, &ec) == 0) {
        can_state_change_cb(can.can_device, cs, ec, NULL);
    }
    k_work_reschedule(&can_led_work, K_SECONDS(2));
}

static void can_state_change_cb(const struct device *dev, enum can_state cs,
                                struct can_bus_err_cnt err_cnt, void *user_data)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);

    switch (cs) {
    case CAN_STATE_ERROR_ACTIVE:
        status_led_set(STATUS_LED_OPERATIONAL);
        break;
    case CAN_STATE_ERROR_WARNING:
    case CAN_STATE_ERROR_PASSIVE:
        status_led_set(STATUS_LED_WARNING);
        break;
    case CAN_STATE_BUS_OFF:
        status_led_set(STATUS_LED_BUS_OFF);
        break;
    default:
        break;
    }
}

static void fccu_can_init()
{
    status_led_init(&can.can_status_led, NULL);
    status_led_set(STATUS_LED_INIT);
    can_set_state_change_callback(can.can_device, can_state_change_cb, NULL);
    can_init(can.can_device, 500000);
    k_work_reschedule(&can_led_work, K_SECONDS(2));
}

static void counter_cb_measurements(const struct device *dev, void *user_data)
{
    flags.measurements_tick = true;
}

static void fccu_counters_init()
{
    counter_init(counter.counter_measurements);
}

static void fccu_counters_set_interrupts()
{
    counter_set_alarm(counter.counter_measurements, 0,
                      counter_cb_measurements, 1000000);
}

static void fccu_can_send_state()
{
    struct candef_fccu_state_t msg = {
        .running_state = (uint8_t)state,
        .main_valve    = flags.main_valve_on  ? 1u : 0u,
        .purge_valve   = flags.purge_valve_on ? 1u : 0u,
        .fan_on        = flags.fan_on         ? 1u : 0u,
        .fan_duty_pct  = fan_pwm_percent,
    };
    uint8_t buf[CANDEF_FCCU_STATE_LENGTH];
    candef_fccu_state_pack(buf, &msg, sizeof(buf));
    can_send_(can.can_device, CANDEF_FCCU_STATE_FRAME_ID, buf, sizeof(buf));
}

void fccu_init()
{
    flags.measurements_tick = false;
    flags.purge_valve_on    = false;

    fccu_adc_init();
    fccu_ads1015_init();
    fccu_can_init();
    fccu_valves_init();
    fccu_buttons_init();
    fccu_fan_init();
    fccu_flow_init();
    fccu_counters_init();
    fccu_counters_set_interrupts();
    fccu_bmp280_sensor_init();
    fccu_bmp280_sensor2_init();
}

void fccu_on_tick()
{
    if (flags.measurements_tick) {
        fccu_bmp280_sensor_read();
        fccu_bmp280_sensor2_read();
        fccu_adc_read();
        fccu_ads1015_read();
        fccu_flow_on_tick();
        fccu_can_send_state();
        fccu_flow_can_send();

        if (g_fan_manual) {
            fan_pwm_percent = g_fan_manual_duty_pct;
        } else if (state == RUNNING) {
            fan_pwm_percent = fccu_fan_compute_duty(sensor.temperature);
        } else {
            fan_pwm_percent = 0;
        }

        if (fan_pwm_percent > 0 && !flags.fan_on) {
            fccu_fan_on();
        } else if (fan_pwm_percent == 0 && flags.fan_on) {
            fccu_fan_off();
        }
        fccu_fan_pwm_set(fan_pwm_percent);

        if (state == RUNNING) {
            purge_periodic_ticks++;

            if (!flags.purge_valve_on) {
                switch (g_purge_mode) {
                case PURGE_MODE_THRESHOLD: {
                    float fc_past;
                    if (fccu_log_get_fc_ago(PURGE_COMPARE_SAMPLES, &fc_past) &&
                        fc_past - adc.fuel_cell_voltage.voltage >= g_purge_trigger_v) {
                        LOG_INF("Threshold purge: FC drop %.2f V",
                                (double)(fc_past - adc.fuel_cell_voltage.voltage));
                        fccu_purge_valve_on();
                    }
                    break;
                }
                case PURGE_MODE_PERIODIC:
                    if (purge_periodic_ticks >= g_purge_periodic_interval_s) {
                        LOG_INF("Periodic purge (%u s)", g_purge_periodic_interval_s);
                        fccu_purge_valve_on();
                        purge_periodic_ticks = 0;
                    }
                    break;
                case PURGE_MODE_MANUAL:
                    break;
                }
            }
        }

        fccu_log_sample_t s = {
            .ts_ms     = k_uptime_get(),
            .fc_v      = adc.fuel_cell_voltage.voltage,
            .sc_v      = adc.supercap_voltage.voltage,
            .temp_fc_c = ads1015_data.fc_temp_c,
            .lp_bar    = ads1015_data.lp_sensor,
            .bme76_t   = sensor.temperature,
            .bme76_h   = sensor.humidity,
            .bme76_p   = sensor.pressure,
            .bme77_t   = sensor2.temperature,
            .bme77_h   = sensor2.humidity,
            .bme77_p   = sensor2.pressure,
        };
        for (int i = 0; i < 4; i++) {
            s.ads48[i] = ads1015_data.ads48[i];
            s.ads49[i] = ads1015_data.ho_current[i];
        }
        fccu_log_add(&s);

        flags.measurements_tick = false;
    }

    k_msleep(100);
}
