#ifndef ADS1015_H
#define ADS1015_H

#include <stdint.h>
#include <zephyr/drivers/i2c.h>
#include <string.h>

#define ADS1015_I2C_ADDRESS 0x48

typedef enum {
    CONVERSION_REGISTER = 0x00,
    CONFIG_REGISTER = 0x01,
    LOW_TRESH_REGISTER = 0x02,
    HIGH_TRESH_REGISTER = 0x03,
} ads1015_addr_ptr_reg_t;

typedef enum {
    FSR_6_144V = 0x00,
    FSR_4_096V = 0x01,
    FSR_2_048V = 0x02,
    FSR_1_024V = 0x03,
    FSR_0_512V = 0x04,
    FSR_0_256V = 0x05,
} ads1015_prog_gain_amplifier_t;

typedef enum {
    SPS_128 = 0x00,
    SPS_250 = 0x01,
    SPS_490 = 0x02,
    SPS_920 = 0x03,
    SPS_1600 = 0x04,
    SPS_2400 = 0x05,
    SPS_3300 = 0x06,
} ads1015_data_rate_t;

typedef enum {
    AIN0_AIN1 = 0x00,
    AIN0_AIN3 = 0x01,
    AIN1_AIN3 = 0x02,
    AIN2_AIN3 = 0x03,
    AIN0 = 0x04,
    AIN1 = 0x05,
    AIN2 = 0x06,
    AIN3 = 0x07,
} ads1015_input_mux_config_t;

typedef enum {
    ADC_CHANNEL_0,
    ADC_CHANNEL_1,
    ADC_CHANNEL_2,
    ADC_CHANNEL_3,
} ads1015_adc_channel_t;

typedef enum {
    CONTINOUS_CONVERSION = 0x00,
    SINGLE_SHOT = 0x01,
} ads1015_device_mode_t;

typedef enum {
    TRADITIONAL = 0x00,
    WINDOW = 0x01,
} ads1015_comp_mode_t;

typedef enum {
    ACTIVE_LOW = 0x00,
    ACTIVE_HIGH = 0x01,
} ads1015_comp_polarity_t;

typedef enum {
    NONLATCHING = 0x00,
    LATCHING = 0x01,
} ads1015_comp_latching_t;

typedef enum {
    AFTER_1_CONV = 0x00,
    AFTER_2_CONV = 0x01,
    AFTER_4_CONV = 0x02,
    DISABLE = 0x03,
} ads1015_comp_queue_t;

typedef enum {
    IDLE = 0x00,
    START_SINGLE_CONV = 0x01,
} operational_status_write_t;

typedef enum {
    CONV_IN_PROGRESS,
    CONV_DONE,
} operational_status_read_t;

typedef struct
{
    ads1015_comp_mode_t comp_mode : 1;
    ads1015_comp_polarity_t comp_polarity : 1;
    ads1015_comp_latching_t comp_latching : 1;
    ads1015_comp_queue_t comp_queue : 2;
} ads1015_comp_t;

typedef struct
{
    ads1015_input_mux_config_t mux_config : 3;
    ads1015_prog_gain_amplifier_t gain : 3;
    ads1015_device_mode_t device_mode : 1;
    ads1015_data_rate_t data_rate : 3;
} ads1015_config_t;

typedef struct
{
    uint16_t i2c_addr;
    ads1015_config_t config;
    ads1015_comp_t comparator;
} ads1015_type_t;

void ads1015_init(ads1015_type_t* ads1015_dev);
void ads1015_write_reg(ads1015_type_t* ads1015_dev, uint8_t reg, uint8_t* data, size_t data_len);
void ads1015_read_reg(ads1015_type_t* ads1015_dev, uint8_t reg, uint8_t* data_read, size_t data_len);
int16_t ads1015_read_channel_raw_single_shot(ads1015_type_t* ads1015_dev, ads1015_adc_channel_t channel);
float ads1015_convert_raw_value_to_voltage(ads1015_type_t* ads1015_dev, int16_t raw_value);
float ads1015_read_channel_single_shot(ads1015_type_t* ads1015_dev, ads1015_adc_channel_t channel);

#endif //ADS1015_H