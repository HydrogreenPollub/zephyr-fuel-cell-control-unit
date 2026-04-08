#include "status_led.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(status_led, LOG_LEVEL_INF);

#define THREAD_STACK_SIZE 1024
#define THREAD_PRIORITY   7

/* CiA 303-3 timing (ms) */
#define FLASH_ON_MS  200
#define FLASH_GAP_MS 200
#define SEQ_GAP_MS   1000

K_THREAD_STACK_DEFINE(status_led_stack, THREAD_STACK_SIZE);
static struct k_thread status_led_thread_data;

static const struct gpio_dt_spec *led_spec;
static status_led_broadcast_t    broadcast_cb;
static atomic_t current_state    = ATOMIC_INIT(STATUS_LED_INIT);
static atomic_t override_active  = ATOMIC_INIT(0);

static void led_on(void)  { gpio_pin_set_dt(led_spec, 1); }
static void led_off(void) { gpio_pin_set_dt(led_spec, 0); }

static void flash(int count) {
    for (int i = 0; i < count; i++) {
        led_on();
        k_sleep(K_MSEC(FLASH_ON_MS));
        led_off();
        if (i < count - 1) {
            k_sleep(K_MSEC(FLASH_GAP_MS));
        }
    }
    k_sleep(K_MSEC(SEQ_GAP_MS));
}

static void run_pattern(status_led_state_t state) {
    if (atomic_get(&override_active)) {
        led_on();
        k_sleep(K_MSEC(100));
        return;
    }

    switch (state) {
    case STATUS_LED_INIT:
        led_on();
        k_sleep(K_MSEC(FLASH_ON_MS));
        led_off();
        k_sleep(K_MSEC(FLASH_GAP_MS));
        break;
    case STATUS_LED_OPERATIONAL:
        flash(1);
        break;
    case STATUS_LED_WARNING:
        flash(2);
        break;
    case STATUS_LED_BUS_OFF:
        led_on();
        k_sleep(K_MSEC(FLASH_ON_MS));
        break;
    case STATUS_LED_DFU:
        flash(3);
        break;
    default:
        k_sleep(K_MSEC(200));
        break;
    }
}

static void status_led_thread(void *p1, void *p2, void *p3) {
    uint32_t last_broadcast_ms = 0;
    LOG_INF("Status LED thread started");

    while (1) {
        status_led_state_t state = (status_led_state_t)atomic_get(&current_state);
        run_pattern(state);

        if (broadcast_cb) {
            uint32_t now = k_uptime_get_32();
            if (now - last_broadcast_ms >= 1000U) {
                broadcast_cb(state);
                last_broadcast_ms = now;
            }
        }
    }
}

void status_led_init(const struct gpio_dt_spec *led,
                     status_led_broadcast_t cb) {
    led_spec     = led;
    broadcast_cb = cb;

    gpio_pin_configure_dt(led_spec, GPIO_OUTPUT_INACTIVE);

    k_tid_t tid = k_thread_create(
        &status_led_thread_data, status_led_stack,
        K_THREAD_STACK_SIZEOF(status_led_stack),
        status_led_thread, NULL, NULL, NULL,
        THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(tid, "status_led");

    LOG_INF("Status LED initialized");
}

void status_led_set(status_led_state_t state) {
    atomic_set(&current_state, (atomic_val_t)state);
}

status_led_state_t status_led_get(void) {
    return (status_led_state_t)atomic_get(&current_state);
}

void status_led_set_override(bool on) {
    atomic_set(&override_active, on ? 1 : 0);
    if (on) {
        led_on();
        k_wakeup(&status_led_thread_data);
    } else {
        led_off();
    }
}
