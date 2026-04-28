#ifndef FCCU_DIGITAL_H
#define FCCU_DIGITAL_H

#include "fccu.h"

/**
 * @brief Purge valve trigger mode.
 */
typedef enum {
    PURGE_MODE_THRESHOLD, /**< Purge when FC voltage drops by g_purge_trigger_v. */
    PURGE_MODE_PERIODIC,  /**< Purge at a fixed interval defined by g_purge_periodic_interval_s. */
    PURGE_MODE_MANUAL,    /**< Purge only on explicit command (shell or button short-press). */
} fccu_purge_mode_t;

extern fccu_valve_pin_t   valve_pin;                  /**< Valve GPIO pin specs. */
extern float              g_purge_trigger_v;           /**< FC voltage drop threshold for PURGE_MODE_THRESHOLD (V). */
extern fccu_purge_mode_t  g_purge_mode;                /**< Active purge trigger mode. */
extern uint32_t           g_purge_periodic_interval_s; /**< Interval between periodic purges (s). */

/**
 * @brief Initialise main and purge valve GPIO pins to inactive state.
 *
 * Configures both valve pins as inactive outputs and initialises the
 * delayable work item used to auto-close the purge valve after a pulse.
 */
void fccu_valves_init();

/**
 * @brief Configure GPIO interrupt callbacks for the onboard and external buttons.
 *
 * The onboard button uses a 20 ms software debounce and triggers a 2-second
 * LED test on confirmed press. The external button detects short press
 * (manual purge) and long press (>500 ms) to toggle the running state and
 * automatic purge mode.
 */
void fccu_buttons_init();

/**
 * @brief Open the main hydrogen supply valve.
 *
 * Asserts the main valve GPIO and sets flags.main_valve_on.
 */
void fccu_main_valve_on();

/**
 * @brief Close the main hydrogen supply valve.
 *
 * De-asserts the main valve GPIO and clears flags.main_valve_on.
 */
void fccu_main_valve_off();

/**
 * @brief Pulse the purge valve for the default duration.
 *
 * Convenience wrapper that calls fccu_purge_valve_on_ms() with PURGE_DURATION_MS.
 */
void fccu_purge_valve_on();

/**
 * @brief Pulse the purge valve for a custom duration.
 *
 * Opens the purge valve immediately and schedules a delayable work item to
 * close it after @p ms milliseconds. Calling this while a pulse is already
 * active reschedules the close deadline.
 *
 * @param ms Pulse duration in milliseconds. Must be greater than 0.
 */
void fccu_purge_valve_on_ms(uint32_t ms);

#endif /* FCCU_DIGITAL_H */
