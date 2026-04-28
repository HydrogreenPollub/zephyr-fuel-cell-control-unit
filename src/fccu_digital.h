#ifndef FCCU_DIGITAL_H
#define FCCU_DIGITAL_H

#include "fccu.h"

extern fccu_valve_pin_t valve_pin;

/* Configurable setpoints (shell-adjustable) */
extern float   g_purge_trigger_v;
extern uint8_t g_main_valve_setpoint_pct;

/* Initialise main and purge valve GPIO pins to inactive. */
void fccu_valves_init();

/* Configure GPIO interrupts for both buttons. */
void fccu_buttons_init();

/* Open the main hydrogen valve. */
void fccu_main_valve_on();

/* Close the main hydrogen valve. */
void fccu_main_valve_off();

/* Pulse the purge valve for PURGE_DURATION_MS milliseconds. */
void fccu_purge_valve_on();

/* Pulse the purge valve for a custom duration in milliseconds. */
void fccu_purge_valve_on_ms(uint32_t ms);

#endif /* FCCU_DIGITAL_H */
