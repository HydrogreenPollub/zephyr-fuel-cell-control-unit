#ifndef FCCU_ANALOG_H
#define FCCU_ANALOG_H

#include "fccu.h"

extern fccu_adc_t         adc;
extern bmp280_sensor_t    sensor;
extern bmp280_sensor_t    sensor2;
extern ads1015_adc_data_t ads1015_data;
extern float              ho_zero_v[4];

void fccu_adc_init();
void fccu_adc_read();

void fccu_ads1015_init();
void fccu_ads1015_read();

void fccu_bmp280_sensor_init();
void fccu_bmp280_sensor2_init();
void fccu_bmp280_sensor_read();
void fccu_bmp280_sensor2_read();

#endif /* FCCU_ANALOG_H */
