#include "fccu_digital.h"
#include "fccu_fan.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fccu_digital, LOG_LEVEL_INF);

fccu_valve_pin_t valve_pin = {
    .main_valve_on_pin  = GPIO_DT_SPEC_GET(DT_ALIAS(main_valve_pin), gpios),
    .purge_valve_on_pin = GPIO_DT_SPEC_GET(DT_ALIAS(purge_valve_pin), gpios),
};

float   g_purge_trigger_v         = PURGE_TRIGGER_FC_DROP_V;
uint8_t g_main_valve_setpoint_pct = MAIN_VALVE_DEFAULT_PCT;

static struct k_work_delayable purge_valve_off_work;

static void purge_valve_off_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    gpio_reset(&valve_pin.purge_valve_on_pin);
    flags.purge_valve_on = false;
}

void fccu_valves_init()
{
    gpio_init(&valve_pin.main_valve_on_pin,  GPIO_OUTPUT_INACTIVE);
    gpio_init(&valve_pin.purge_valve_on_pin, GPIO_OUTPUT_INACTIVE);
    k_work_init_delayable(&purge_valve_off_work, purge_valve_off_fn);
    flags.main_valve_on  = false;
    flags.purge_valve_on = false;
}

void fccu_main_valve_on()
{
    gpio_set(&valve_pin.main_valve_on_pin);
    flags.main_valve_on = true;
    LOG_INF("Main valve on");
}

void fccu_main_valve_off()
{
    gpio_reset(&valve_pin.main_valve_on_pin);
    flags.main_valve_on = false;
    LOG_INF("Main valve off");
}

void fccu_purge_valve_on_ms(uint32_t ms)
{
    gpio_set(&valve_pin.purge_valve_on_pin);
    flags.purge_valve_on = true;
    k_work_reschedule(&purge_valve_off_work, K_MSEC(ms));
}

void fccu_purge_valve_on()
{
    fccu_purge_valve_on_ms(PURGE_DURATION_MS);
}

static const struct gpio_dt_spec btn_onboard  =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), button_onboard_gpios);
static const struct gpio_dt_spec btn_external =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), button_external_gpios);

static struct gpio_callback onboard_cb_data;
static struct gpio_callback external_cb_data;

static void led_test_off_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(led_test_off_work, led_test_off_fn);

static void led_test_off_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    status_led_set_override(false);
}

static void on_onboard_press()
{
    LOG_INF("On-board button: LED test");
    status_led_set_override(true);
    k_work_reschedule(&led_test_off_work, K_SECONDS(2));
}

static bool ext_long_fired;

static void external_long_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(external_long_work, external_long_fn);

static void external_long_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    ext_long_fired = true;

    if (state == RUNNING) {
        LOG_INF("External button: long press — stopping");
        state = STOPPED;
        fccu_main_valve_off();
        fccu_fan_off();
    } else {
        LOG_INF("External button: long press — starting");
        state = RUNNING;
        fccu_main_valve_on();
        fccu_fan_on();
        fan_pwm_percent = FAN_MIN_DUTY_PCT;
        fccu_fan_pwm_set(fan_pwm_percent);
    }
}

static void onboard_gpio_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);
    on_onboard_press();
}

static void external_gpio_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);

    if (gpio_pin_get_dt(&btn_external) > 0) {
        ext_long_fired = false;
        k_work_reschedule(&external_long_work, K_MSEC(500));
    } else {
        k_work_cancel_delayable(&external_long_work);
        if (!ext_long_fired) {
            LOG_INF("External button: short press — manual purge 100 ms");
            if (!flags.purge_valve_on) {
                fccu_purge_valve_on_ms(100);
            }
        }
    }
}

void fccu_buttons_init()
{
    gpio_pin_configure_dt(&btn_onboard, GPIO_INPUT);
    gpio_init_callback(&onboard_cb_data, onboard_gpio_cb, BIT(btn_onboard.pin));
    gpio_add_callback(btn_onboard.port, &onboard_cb_data);
    gpio_pin_interrupt_configure_dt(&btn_onboard, GPIO_INT_EDGE_TO_ACTIVE);
    LOG_INF("On-board button configured (GPIO %d)", btn_onboard.pin);

    gpio_pin_configure_dt(&btn_external, GPIO_INPUT);
    gpio_init_callback(&external_cb_data, external_gpio_cb, BIT(btn_external.pin));
    gpio_add_callback(btn_external.port, &external_cb_data);
    gpio_pin_interrupt_configure_dt(&btn_external, GPIO_INT_EDGE_BOTH);
    LOG_INF("External button configured (GPIO %d)", btn_external.pin);
}
