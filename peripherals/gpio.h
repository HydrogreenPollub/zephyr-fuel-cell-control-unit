#ifndef GPIO_H
#define GPIO_H

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

int gpio_init(struct gpio_dt_spec *gpio, gpio_flags_t extra_flags);
int gpio_reset(struct gpio_dt_spec *gpio);
int gpio_set(struct gpio_dt_spec *gpio);
int gpio_set_interrupt(struct gpio_dt_spec *gpio, gpio_flags_t flags, struct gpio_callback *gpio_cb_data, gpio_callback_handler_t handler);

#endif //GPIO_H
