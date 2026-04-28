#ifndef FCCU_FAN_H
#define FCCU_FAN_H

#include "fccu.h"

extern fccu_fan_t fan;            /**< Fan GPIO and PWM hardware resources. */
extern uint8_t   fan_pwm_percent; /**< Current fan PWM duty cycle (0–100 %). */

extern float   g_fan_target_c;       /**< Fan proportional controller setpoint (°C). */
extern bool    g_fan_manual;          /**< True when fan is in manual duty override mode. */
extern uint8_t g_fan_manual_duty_pct; /**< Manual duty cycle (0–100 %). Used when g_fan_manual is true. */

/**
 * @brief Initialise the fan GPIO enable pin and PWM output.
 *
 * Configures the fan enable pin as an inactive output, sets PWM duty to 0 %,
 * and clears flags.fan_on.
 */
void fccu_fan_init();

/**
 * @brief Assert the fan enable GPIO to power the fan driver.
 *
 * Sets flags.fan_on. The PWM duty must also be non-zero for the fan to spin.
 */
void fccu_fan_on();

/**
 * @brief De-assert the fan enable GPIO to cut power to the fan driver.
 *
 * Clears flags.fan_on regardless of the current PWM duty cycle.
 */
void fccu_fan_off();

/**
 * @brief Set the fan PWM duty cycle.
 *
 * Applies the given percentage to the LEDC PWM channel configured in the
 * devicetree. Does not affect the enable GPIO.
 *
 * @param pwm_percent Duty cycle in percent (0–100).
 */
void fccu_fan_pwm_set(uint8_t pwm_percent);

/**
 * @brief Compute the proportional fan duty cycle from a temperature reading.
 *
 * Returns 0 if @p temp_c is below (FAN_TARGET_C - FAN_DEADBAND_C).
 * Returns FAN_MIN_DUTY_PCT when within the deadband.
 * Scales linearly to 100 % as temperature rises to FAN_TARGET_C + FAN_FULLSCALE_C.
 *
 * @param temp_c Current temperature in degrees Celsius (typically from BME280@76).
 * @return Computed duty cycle clamped to 0–100 %.
 */
uint8_t fccu_fan_compute_duty(float temp_c);

#endif /* FCCU_FAN_H */
