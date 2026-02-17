#include "pwm.h"

void pwm_init(struct pwm_dt_spec *pwm) {
    if (!pwm_is_ready_dt(pwm)) {
        printf("Error: PWM device %s is not ready\n", pwm->dev->name);
    }
}

int pwm_set_pulse_width_percent(struct pwm_dt_spec *pwm, uint8_t pulse_width_percent) {
    if (pulse_width_percent > 100) pulse_width_percent = 100;

    uint32_t pulse_width = (pwm->period * pulse_width_percent) / 100;

    int ret = pwm_set_pulse_dt(pwm, pulse_width);
    if (ret < 0) {
        printf("Error %d: failed to set pulse width\n", ret);
    }
    return ret;
}