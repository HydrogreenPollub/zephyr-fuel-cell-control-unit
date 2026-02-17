#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <stdio.h>
#include "can.h"
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>

#include "../../zephyr/include/zephyr/devicetree.h"
#include <zephyr/logging/log.h>

#include "fccu_v2.h"

LOG_MODULE_REGISTER(app);

int main() {
    LOG_INF("Starting program on board: %s\n", CONFIG_BOARD);

    fccu_init();

    while (1) {
        fccu_on_tick();
    }
    return 0;
}
