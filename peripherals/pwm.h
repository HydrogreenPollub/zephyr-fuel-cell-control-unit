#ifndef PWM_H
#define PWM_H

#include <zephyr/drivers/pwm.h>

void pwm_init(struct pwm_dt_spec *pwm);
int pwm_set_pulse_width_percent(struct pwm_dt_spec *pwm, uint8_t pulse_width_percent);


#endif //PWM_H
