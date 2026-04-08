#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <stdbool.h>
#include <zephyr/drivers/gpio.h>

/* CiA 303-3 inspired node status states.
 * LED patterns (all periods ~2 s):
 *   INIT        — blinking 2.5 Hz (not yet operational)
 *   OPERATIONAL — single flash
 *   WARNING     — double flash  (CAN error counters elevated)
 *   BUS_OFF     — steady on     (CAN bus-off)
 *   DFU         — triple flash  (firmware update in progress)
 */
typedef enum {
    STATUS_LED_INIT        = 0,
    STATUS_LED_OPERATIONAL = 1,
    STATUS_LED_WARNING     = 2,
    STATUS_LED_BUS_OFF     = 3,
    STATUS_LED_DFU         = 4,
} status_led_state_t;

/* Called once per second from the status LED thread with the current state.
 * Implement per-board to broadcast state over CAN or another transport.
 * May be NULL if no broadcast is needed. */
typedef void (*status_led_broadcast_t)(status_led_state_t state);

/* Initialise the status LED and start its driving thread.
 * led          — GPIO spec for the status LED pin.
 * broadcast_cb — optional; called every 1 s with the current state. */
void status_led_init(const struct gpio_dt_spec *led,
                     status_led_broadcast_t broadcast_cb);

void status_led_set(status_led_state_t state);
status_led_state_t status_led_get(void);

/* While override is active the thread yields and the LED stays on (test use). */
void status_led_set_override(bool on);

#endif /* STATUS_LED_H */
