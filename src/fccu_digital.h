#ifndef FCCU_DIGITAL_H
#define FCCU_DIGITAL_H

#include "fccu.h"

extern fccu_valve_pin_t valve_pin;

void fccu_valves_init();
void fccu_buttons_init();
void fccu_main_valve_on();
void fccu_main_valve_off();
void fccu_purge_valve_on();
void fccu_purge_valve_on_ms(uint32_t ms);

#endif /* FCCU_DIGITAL_H */
