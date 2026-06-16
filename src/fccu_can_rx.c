#include "fccu_can_rx.h"
#include "fccu.h"
#include "can.h"
#include "candef.h"

#include <zephyr/drivers/can.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fccu_can_rx, LOG_LEVEL_INF);

fccu_mcu_data_t mcu_data;

static void rx_fuel_cell_cb(const struct device *dev, struct can_frame *frame, void *user_data)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);

    struct candef_mcu_analog_fuel_cell_t msg;
    if (candef_mcu_analog_fuel_cell_unpack(&msg, frame->data, frame->dlc) != 0) {
        return;
    }
    mcu_data.fc_v = (float)candef_mcu_analog_fuel_cell_fuel_cell_output_voltage_decode(
        msg.fuel_cell_output_voltage);
    mcu_data.fc_c = (float)candef_mcu_analog_fuel_cell_fuel_cell_output_current_decode(
        msg.fuel_cell_output_current);
    mcu_data.hp_bar = (float)candef_mcu_analog_fuel_cell_hydrogen_high_pressure_decode(
        msg.hydrogen_high_pressure);
    mcu_data.leakage_v = (float)candef_mcu_analog_fuel_cell_hydrogen_leakage_sensor_voltage_decode(
        msg.hydrogen_leakage_sensor_voltage);
    mcu_data.last_rx_ms = k_uptime_get();
}

static void rx_powertrain_cb(const struct device *dev, struct can_frame *frame, void *user_data)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);

    struct candef_mcu_analog_powertrain_t msg;
    if (candef_mcu_analog_powertrain_unpack(&msg, frame->data, frame->dlc) != 0) {
        return;
    }
    mcu_data.sc_v = (float)candef_mcu_analog_powertrain_supercapacitor_voltage_decode(
        msg.supercapacitor_voltage);
    mcu_data.sc_c = (float)candef_mcu_analog_powertrain_supercapacitor_current_decode(
        msg.supercapacitor_current);
    mcu_data.mc_v = (float)candef_mcu_analog_powertrain_motor_controller_supply_voltage_decode(
        msg.motor_controller_supply_voltage);
    mcu_data.mc_c = (float)candef_mcu_analog_powertrain_motor_controller_supply_current_decode(
        msg.motor_controller_supply_current);
    mcu_data.last_rx_ms = k_uptime_get();
}

void fccu_can_rx_init()
{
    static const struct can_filter filter_fc = {
        .id    = CANDEF_MCU_ANALOG_FUEL_CELL_FRAME_ID,
        .mask  = CAN_EXT_ID_MASK,
        .flags = CAN_FRAME_IDE,
    };
    static const struct can_filter filter_pt = {
        .id    = CANDEF_MCU_ANALOG_POWERTRAIN_FRAME_ID,
        .mask  = CAN_EXT_ID_MASK,
        .flags = CAN_FRAME_IDE,
    };

    can_add_rx_filter_(can.can_device, rx_fuel_cell_cb, &filter_fc);
    can_add_rx_filter_(can.can_device, rx_powertrain_cb, &filter_pt);

    LOG_INF("CAN RX: listening on 0x%03x (FC) and 0x%03x (powertrain)",
            CANDEF_MCU_ANALOG_FUEL_CELL_FRAME_ID, CANDEF_MCU_ANALOG_POWERTRAIN_FRAME_ID);
}
