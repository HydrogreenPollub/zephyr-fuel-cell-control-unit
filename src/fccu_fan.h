#ifndef FCCU_FAN_H
#define FCCU_FAN_H

#include "fccu.h"

extern fccu_fan_t fan;
extern uint8_t   fan_pwm_percent;

extern float   g_fan_target_c;
extern bool    g_fan_manual;
extern uint8_t g_fan_manual_duty_pct;

/* Initialise fan GPIO and PWM to inactive. */
void fccu_fan_init();

/* Assert the fan enable GPIO. */
void fccu_fan_on();

/* De-assert the fan enable GPIO. */
void fccu_fan_off();

/* Set PWM duty cycle (0–100 %). */
void fccu_fan_pwm_set(uint8_t pwm_percent);

/* Compute proportional duty cycle from BME76 temperature.
 * Returns FAN_MIN_DUTY_PCT when within deadband, 100 % at target + FAN_FULLSCALE_C. */
uint8_t fccu_fan_compute_duty(float temp_c);

#endif /* FCCU_FAN_H */
