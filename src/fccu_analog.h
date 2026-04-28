#ifndef FCCU_ANALOG_H
#define FCCU_ANALOG_H

#include "fccu.h"

extern fccu_adc_t         adc;          /**< Native ESP32 ADC measurement data. */
extern bmp280_sensor_t    sensor;       /**< BME280 at I2C address 0x76. */
extern bmp280_sensor_t    sensor2;      /**< BME280 at I2C address 0x77. */
extern ads1015_adc_data_t ads1015_data; /**< Most recent ADS1015 measurements. */
extern float              ho_zero_v[4]; /**< HO-10P zero-current calibration offsets (V), one per channel. */

/**
 * @brief Initialise the native ESP32 ADC channels.
 *
 * Calls adc_init() for the fuel cell voltage, supercapacitor voltage, and NTC
 * temperature channels defined in the devicetree zephyr,user node.
 */
void fccu_adc_init();

/**
 * @brief Read all native ADC channels and publish telemetry CAN frames.
 *
 * Reads fuel cell voltage, supercap voltage, and temperature; applies
 * moving-average filtering; then sends FCCU_POWER and FCCU_HYDROGEN CAN frames.
 */
void fccu_adc_read();

/**
 * @brief Probe ADS1015 ICs on I2C and run HO-10P zero calibration.
 *
 * Attempts to contact ADS1015@48 and ADS1015@49. Devices that respond are
 * initialised and their detection flags set. If ADS1015@49 is present,
 * performs a blocking HO_ZERO_CAL_SAMPLES-point zero calibration.
 */
void fccu_ads1015_init();

/**
 * @brief Read both ADS1015 ICs and publish the FCCU_CURRENTS CAN frame.
 *
 * Skips a device if it was not detected during init. Applies moving-average
 * filtering and derives fc_current, fc_temp_c, hp_sensor, lp_sensor, and
 * ho_current from the raw channel voltages.
 */
void fccu_ads1015_read();

/**
 * @brief Verify that BME280 at I2C address 0x76 is ready.
 *
 * Logs an error if the Zephyr sensor driver did not initialise the device
 * successfully. Does not perform any I2C transaction.
 */
void fccu_bmp280_sensor_init();

/**
 * @brief Verify that BME280 at I2C address 0x77 is ready.
 *
 * Logs an error if the Zephyr sensor driver did not initialise the device
 * successfully. Does not perform any I2C transaction.
 */
void fccu_bmp280_sensor2_init();

/**
 * @brief Fetch one sample from BME280@76 and update the sensor struct.
 *
 * Calls sensor_sample_fetch() and reads temperature, pressure, and humidity
 * into sensor.temperature / .pressure / .humidity with moving-average smoothing.
 * On I2C failure the bus is recovered via i2c_recover_bus() and the function
 * returns without updating data.
 */
void fccu_bmp280_sensor_read();

/**
 * @brief Fetch one sample from BME280@77, update sensor2, and send FCCU_THERMAL.
 *
 * Calls sensor_sample_fetch() and reads all channels into sensor2 with
 * moving-average smoothing. On I2C failure the bus is recovered. On success,
 * sends a FCCU_THERMAL CAN frame with temperature and humidity from both sensors.
 */
void fccu_bmp280_sensor2_read();

#endif /* FCCU_ANALOG_H */
