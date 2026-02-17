#ifndef COUNTER_H
#define COUNTER_H
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/logging/log.h>

void counter_init(const struct device *counter_dev);
// void counter_set_alarm(const struct device *counter_dev, uint8_t channel_id, counter_alarm_callback_t callback, uint32_t microseconds);
void counter_set_alarm(const struct device *counter_dev, uint8_t channel_id, counter_top_callback_t callback, uint32_t microseconds);

#endif //COUNTER_H
