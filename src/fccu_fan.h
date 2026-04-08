#ifndef FCCU_FAN_H
#define FCCU_FAN_H

#include "fccu.h"

extern fccu_fan_t fan;
extern uint8_t   fan_pwm_percent;

extern float   g_fan_target_c;
extern bool    g_fan_manual;
extern uint8_t g_fan_manual_duty_pct;

void    fccu_fan_init();
void    fccu_fan_on();
void    fccu_fan_off();
void    fccu_fan_pwm_set(uint8_t pwm_percent);
uint8_t fccu_fan_compute_duty(float temp_c);

#endif /* FCCU_FAN_H */
