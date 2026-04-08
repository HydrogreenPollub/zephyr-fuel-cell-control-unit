#include "fccu_fan.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fccu_fan, LOG_LEVEL_INF);

fccu_fan_t fan = {
    .fan_on_pin = GPIO_DT_SPEC_GET(DT_ALIAS(fan_pin), gpios),
    .fan_pwm    = PWM_DT_SPEC_GET(DT_ALIAS(fan_pwm)),
};

uint8_t fan_pwm_percent   = 0;
float   g_fan_target_c    = FAN_TARGET_C;
bool    g_fan_manual      = false;
uint8_t g_fan_manual_duty_pct = 0;

void fccu_fan_init()
{
    gpio_init(&fan.fan_on_pin, GPIO_OUTPUT_INACTIVE);
    pwm_init(&fan.fan_pwm);
    flags.fan_on = false;
}

void fccu_fan_on()
{
    gpio_set(&fan.fan_on_pin);
    flags.fan_on = true;
    LOG_INF("Fan on");
}

void fccu_fan_off()
{
    gpio_reset(&fan.fan_on_pin);
    flags.fan_on = false;
    LOG_INF("Fan off");
}

void fccu_fan_pwm_set(uint8_t pwm_percent)
{
    pwm_set_pulse_width_percent(&fan.fan_pwm, pwm_percent);
}

uint8_t fccu_fan_compute_duty(float temp_c)
{
    if (temp_c <= (g_fan_target_c + FAN_DEADBAND_C)) {
        return FAN_MIN_DUTY_PCT;
    }
    float k = (temp_c - g_fan_target_c) / FAN_FULLSCALE_C;
    if (k < 0.0f) k = 0.0f;
    if (k > 1.0f) k = 1.0f;
    return (uint8_t)(FAN_MIN_DUTY_PCT + k * (100 - FAN_MIN_DUTY_PCT));
}
