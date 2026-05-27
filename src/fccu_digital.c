#include "fccu_digital.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fccu_digital, LOG_LEVEL_INF);

fccu_valve_pin_t valve_pin = {
    .main_valve_on_pin  = GPIO_DT_SPEC_GET(DT_ALIAS(main_valve_pin), gpios),
    .purge_valve_on_pin = GPIO_DT_SPEC_GET(DT_ALIAS(purge_valve_pin), gpios),
};

float             g_purge_threshold_v         = PURGE_THRESHOLD_FC_DROP_V;
fccu_purge_mode_t g_purge_mode                = PURGE_MODE_PERIODIC;
uint32_t          g_purge_periodic_interval_s = PURGE_PERIODIC_INTERVAL_S;
uint32_t          g_purge_duration_ms         = PURGE_DURATION_MS;

static struct k_work_delayable purge_valve_off_work;

static void purge_valve_off_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    gpio_reset(&valve_pin.purge_valve_on_pin);
    flags.purge_valve_on = false;
    LOG_INF("Purge valve OFF");
}

void fccu_valves_init()
{
    gpio_init(&valve_pin.main_valve_on_pin, GPIO_OUTPUT_INACTIVE);
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
    LOG_INF("Purge valve ON (%u ms)", ms);
    k_work_reschedule(&purge_valve_off_work, K_MSEC(ms));
}

void fccu_purge_valve_on()
{
    fccu_purge_valve_on_ms(g_purge_duration_ms);
}

static void led_test_off_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(led_test_off_work, led_test_off_fn);

static void led_test_off_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    status_led_set_override(false);
}

static int ext_click_count;

static void ext_click_fn(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(ext_click_work, ext_click_fn);

static void ext_click_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    int clicks      = ext_click_count;
    ext_click_count = 0;

    if (clicks == 1) {
        if (state == RUNNING) {
            LOG_INF("External button: single click — stopping");
            state = STOPPED;
            fccu_main_valve_off();
            g_purge_mode = PURGE_MODE_MANUAL;
        } else {
            LOG_INF("External button: single click — starting");
            state = RUNNING;
            fccu_main_valve_on();
            g_purge_mode = PURGE_MODE_PERIODIC;
        }
    } else if (clicks >= 2) {
        LOG_INF("External button: double click — manual purge");
        if (!flags.purge_valve_on) {
            fccu_purge_valve_on_ms(100);
        }
    }
}

static void button_cb(struct input_event *evt, void *user_data)
{
    ARG_UNUSED(user_data);
    if (evt->type != INPUT_EV_KEY) {
        return;
    }

    if (evt->code == INPUT_KEY_1) {
        if (evt->value) {
            LOG_INF("On-board button: pressed");
            status_led_set_override(true);
            k_work_reschedule(&led_test_off_work, K_SECONDS(2));
        }
    } else if (evt->code == INPUT_KEY_0) {
        if (evt->value) {
            LOG_INF("External button: pressed");
            ext_click_count++;
            k_work_reschedule(&ext_click_work, K_MSEC(400));
        } else {
            LOG_INF("External button: released");
        }
    }
}
INPUT_CALLBACK_DEFINE(NULL, button_cb, NULL);

void fccu_buttons_init()
{
    LOG_INF("Buttons configured via input subsystem");
}
