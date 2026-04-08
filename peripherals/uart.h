//
// Created by inż.Dawid Pisarczyk on 14.02.2026.
//

#ifndef UART_H
#define UART_H
#ifdef __cplusplus
extern "C" {

#endif

#include <stdio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>

int uart_callback_set_(const struct device *dev, uart_callback_t callback);

int uart_rx_init(const struct device *dev, uint8_t *buf, size_t len, int32_t timeout);

int uart_device_init(const struct device *dev);

#ifdef __cplusplus
}
#endif
#endif //UART_H
