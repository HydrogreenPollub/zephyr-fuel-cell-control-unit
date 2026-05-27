#ifndef FCCU_CAN_RX_H
#define FCCU_CAN_RX_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Latest data received from the Master Control Unit over CAN.
 *
 * Populated by RX callbacks for MCU_ANALOG_FUEL_CELL (0x104) and
 * MCU_ANALOG_POWERTRAIN (0x105). All values remain 0 until the first
 * frame of each type arrives.
 */
typedef struct {
    float   fc_v;       /**< Fuel cell output voltage (V). */
    float   fc_c;       /**< Fuel cell output current (A). */
    float   sc_v;       /**< Supercapacitor voltage (V). */
    float   sc_c;       /**< Supercapacitor current (A). */
    float   mc_v;       /**< Motor controller supply voltage (V). */
    float   mc_c;       /**< Motor controller supply current (A). */
    float   leakage_v;  /**< Hydrogen leakage sensor voltage (V). */
    int64_t last_rx_ms; /**< k_uptime_get() timestamp of the most recent frame. */
} fccu_mcu_data_t;

extern fccu_mcu_data_t mcu_data; /**< Latest received MCU telemetry. */

/**
 * @brief Register CAN RX filters for MCU_ANALOG_FUEL_CELL and MCU_ANALOG_POWERTRAIN.
 *
 * Must be called after the CAN controller is started (i.e. after fccu_can_init()).
 */
void fccu_can_rx_init();

#endif /* FCCU_CAN_RX_H */
