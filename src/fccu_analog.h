#ifndef FCCU_ANALOG_H
#define FCCU_ANALOG_H

#include "fccu.h"

extern fccu_adc_t         adc;
extern bmp280_sensor_t    sensor;
extern bmp280_sensor_t    sensor2;
extern ads1015_adc_data_t ads1015_data;
extern float              ho_zero_v[4];

/* Initialise native ESP32 ADC channels. */
void fccu_adc_init();

/* Read all native ADC channels and publish FCCU_POWER and FCCU_HYDROGEN CAN frames. */
void fccu_adc_read();

/* Probe ADS1015 ICs on I2C and run HO-10P zero calibration. */
void fccu_ads1015_init();

/* Read both ADS1015 ICs and publish FCCU_CURRENTS CAN frame. */
void fccu_ads1015_read();

/* Check BME280@76 is ready. */
void fccu_bmp280_sensor_init();

/* Check BME280@77 is ready. */
void fccu_bmp280_sensor2_init();

/* Fetch BME280@76 sample and update sensor.temperature/humidity/pressure. */
void fccu_bmp280_sensor_read();

/* Fetch BME280@77 sample, update sensor2 fields and publish FCCU_THERMAL CAN frame. */
void fccu_bmp280_sensor2_read();

#endif /* FCCU_ANALOG_H */
