#include "fccu.h"
#include "fccu_analog.h"
#include "fccu_fan.h"
#include "fccu_digital.h"
#include "fccu_flow.h"
#include "fccu_log.h"
#include "fccu_settings.h"
#include "fccu_can_rx.h"
#include "can.h"
#include "candef.h"
#include "counter.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fccu, LOG_LEVEL_INF);

volatile fccu_flags_t flags;
volatile fccu_state_t state = STOPPED;

static uint32_t purge_periodic_ticks;
static uint32_t status_can_ticks;
static uint32_t ads_can_ticks = FCCU_CAN_ADS_PERIOD_S - 1U;

fccu_fc_v_source_t g_fc_v_source = FC_V_SOURCE_ADC;

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

static void measurements_work_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(measurements_work, measurements_work_fn);

static void can_state_change_cb(const struct device *dev, enum can_state cs,
                                struct can_bus_err_cnt err_cnt, void *user_data);
static void can_led_work_fn(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(can_led_work, can_led_work_fn);

static void can_led_work_fn(struct k_work *work)
{
    enum can_state         cs;
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
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);
    flags.measurements_tick = true;
}

static void fccu_counters_init()
{
    counter_init(counter.counter_measurements);
}

static void fccu_counters_set_interrupts()
{
    counter_set_alarm(counter.counter_measurements, 0, counter_cb_measurements, 1000000);
}

bool fccu_fc_v_valid()
{
    if (g_fc_v_source == FC_V_SOURCE_ADC) {
        return true;
    }
    return mcu_data.last_rx_ms != 0 &&
           (k_uptime_get() - mcu_data.last_rx_ms) < FC_V_CAN_STALE_MS;
}

float fccu_fc_v_get()
{
    if (g_fc_v_source == FC_V_SOURCE_CAN && fccu_fc_v_valid()) {
        return mcu_data.fc_v;
    }
    return adc.fuel_cell_voltage.voltage;
}

const char *fccu_fc_v_source_name(fccu_fc_v_source_t src)
{
    return src == FC_V_SOURCE_CAN ? "can" : "adc";
}

void fccu_process_measurements()
{
    static int64_t last_ms;
    int64_t        now_ms = k_uptime_get();

    if (now_ms - last_ms < 900) {
        return;
    }
    last_ms = now_ms;

    fccu_bmp280_sensor_read();
    fccu_bmp280_sensor2_read();
    fccu_adc_read();
    fccu_flow_on_tick();

    if (g_fan_manual) {
        fan_pwm_percent = g_fan_manual_duty_pct;
    } else {
        fan_pwm_percent = fccu_fan_compute_duty(sensor.temperature);
    }

    fccu_fan_pwm_set(fan_pwm_percent);

    if (++ads_can_ticks >= FCCU_CAN_ADS_PERIOD_S) {
        fccu_ads1015_read();
        fccu_ads1015_can_send();
        ads_can_ticks = 0;
    }

    if (++status_can_ticks >= FCCU_CAN_STATUS_PERIOD_S) {
        fccu_adc_can_send();
        fccu_bmp280_can_send();
        fccu_flow_can_send();
        status_can_ticks = 0;
    }

    if (state == RUNNING && !flags.purge_valve_on) {
        switch (g_purge_mode) {
        case PURGE_MODE_THRESHOLD: {
            float fc_past;
            float fc_now = fccu_fc_v_get();
            if (fccu_fc_v_valid() && fccu_log_get_fc_ago(PURGE_COMPARE_SAMPLES, &fc_past) &&
                fc_past - fc_now >= g_purge_threshold_v) {
                LOG_INF("Threshold purge: FC drop %.2f V (source=%s)",
                        (double)(fc_past - fc_now), fccu_fc_v_source_name(g_fc_v_source));
                fccu_purge_valve_on();
            }
            break;
        }
        case PURGE_MODE_PERIODIC:
            purge_periodic_ticks++;
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

    fccu_log_sample_t s = {
        .ts_ms     = k_uptime_get(),
        .fc_v      = fccu_fc_v_get(),
        .sc_v      = adc.supercap_voltage.voltage,
        .lp_bar    = ads1015_data.lp_sensor,
        .flow_rate = flow_rate_lnmin,
        .fan_duty  = fan_pwm_percent,
        .ntc_t     = adc.temp_c,
        .bme76_t   = sensor.temperature,
        .bme76_h   = sensor.humidity,
        .bme76_p   = sensor.pressure,
        .bme77_t   = sensor2.temperature,
        .bme77_h   = sensor2.humidity,
        .bme77_p   = sensor2.pressure,
    };
    for (int i = 0; i < 4; i++) {
        s.ho_current[i] = ads1015_data.ho_current[i];
    }
    fccu_log_add(&s);
}

static void measurements_work_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    fccu_process_measurements();
    k_work_reschedule(&measurements_work, K_SECONDS(1));
}

void fccu_can_send_state()
{
    struct candef_fccu_state_t msg = {
        .running_state = (uint8_t)state,
        .main_valve    = flags.main_valve_on ? 1u : 0u,
        .purge_mode    = (uint8_t)g_purge_mode,
        .fan_mode      = g_fan_manual ? 0u : 1u,
        .fan_duty      = fan_pwm_percent,
        .fc_temp_c     = (int16_t)(adc.temp_c * 10.0f),
        .h2_flow       = (uint16_t)(flow_rate_lnmin * 1000.0f),
    };
    uint8_t buf[CANDEF_FCCU_STATE_LENGTH];
    candef_fccu_state_pack(buf, &msg, sizeof(buf));
    can_send_(can.can_device, CANDEF_FCCU_STATE_FRAME_ID, buf, sizeof(buf));
}

void fccu_start()
{
    state = RUNNING;
    fccu_main_valve_on();
    g_purge_mode         = PURGE_MODE_PERIODIC;
    purge_periodic_ticks = 0;
    LOG_INF("System started");
}

void fccu_stop()
{
    state = STOPPED;
    fccu_main_valve_off();
    g_purge_mode         = PURGE_MODE_MANUAL;
    purge_periodic_ticks = 0;
    LOG_INF("System stopped");
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
    fccu_settings_init();
    fccu_fan_apply_saved();
    fccu_can_rx_init();

    fccu_process_measurements();
    k_work_reschedule(&measurements_work, K_SECONDS(1));
}

void fccu_on_tick()
{
    static int64_t fast_can_last_ms;

    if (flags.measurements_tick) {
        fccu_process_measurements();
        flags.measurements_tick = false;
    }

    int64_t now_ms = k_uptime_get();
    if (now_ms - fast_can_last_ms >= FCCU_CAN_FAST_PERIOD_MS) {
        fccu_can_send_state();
        fccu_hydrogen_can_send();
        fast_can_last_ms = now_ms;
    }

    k_msleep(100);
}
