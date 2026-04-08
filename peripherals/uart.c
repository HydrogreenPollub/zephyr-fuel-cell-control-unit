//
// Created by inż.Dawid Pisarczyk on 14.02.2026.
//

#include "uart.h"

LOG_MODULE_REGISTER(uart);

int uart_rx_init(const struct device *dev, uint8_t *buf, size_t len, int32_t timeout) {
    int ret;
    ret = uart_rx_enable(dev, buf, len, timeout);

    if (ret < 0) {
        LOG_ERR("UART receiver not initialized");
    } else if (ret == 0) {
        LOG_INF("UART receiver configured succesfully");
    }
    return ret;
}

int uart_device_init(const struct device *dev) {
    if (!device_is_ready(dev)) {
        LOG_ERR("UART device not ready");
        return -ENODEV;
    }
    LOG_INF("UART device ready");
    return 0;
}

int uart_callback_set_(const struct device *dev, uart_callback_t callback) {
    int ret;
    ret = uart_callback_set(dev, callback, NULL);

    if (ret < 0) {
        LOG_ERR("UART callback not configured");
    } else if (ret == 0) {
        LOG_INF("UART callback configured succesfully");
    }
    return ret;
}
