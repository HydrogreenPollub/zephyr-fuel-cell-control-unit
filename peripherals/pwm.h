#ifndef PWM_H
#define PWM_H
#ifdef __cplusplus
extern "C" {

#endif

#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

void pwm_init(struct pwm_dt_spec *pwm);

int pwm_set_pulse_width_percent(struct pwm_dt_spec *pwm, uint8_t pulse_width_percent);

#ifdef __cplusplus
}
#endif

#endif //PWM_H
