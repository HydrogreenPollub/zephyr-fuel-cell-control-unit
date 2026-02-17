#include "ads1015.h"

#define ADS_NODE DT_NODELABEL(ads1015)
static const struct i2c_dt_spec ads_i2c = I2C_DT_SPEC_GET(ADS_NODE);

void ads1015_init(ads1015_type_t* ads1015_dev)
{
    ads1015_dev->i2c_addr = ADS1015_I2C_ADDRESS;

    ads1015_dev->config.mux_config = AIN0;
    ads1015_dev->config.gain = FSR_4_096V;
    ads1015_dev->config.device_mode = SINGLE_SHOT;
    ads1015_dev->config.data_rate = SPS_1600;
    ads1015_dev->comparator.comp_queue = DISABLE;

    uint8_t config_reg[2];
    config_reg[0]
        = (ads1015_dev->config.mux_config << 4) | (ads1015_dev->config.gain << 1) | ads1015_dev->config.device_mode;
    config_reg[1] = (ads1015_dev->config.data_rate << 5) | (ads1015_dev->comparator.comp_queue);

    ads1015_write_reg(ads1015_dev, CONFIG_REGISTER, config_reg, sizeof(config_reg));
}

void ads1015_write_reg(ads1015_type_t* ads1015_dev, uint8_t reg, uint8_t* data, size_t data_len)
{
    uint8_t data_wr[data_len + 1];
    data_wr[0] = reg;

    memcpy(&data_wr[1], data, data_len);

    i2c_write_dt(&ads_i2c, data_wr, data_len + 1);
}

void ads1015_read_reg(ads1015_type_t* ads1015_dev, uint8_t reg, uint8_t* data_read, size_t data_len)
{
    uint8_t data_wr = reg;
    i2c_write_read_dt(&ads_i2c, &data_wr, 1, data_read, data_len);
}

int16_t ads1015_read_channel_raw_single_shot(ads1015_type_t* ads1015_dev, ads1015_adc_channel_t channel)
{
    if (channel > 3)
        channel = 3;

    uint8_t adc_single_ended[] = {
        AIN0,
        AIN1,
        AIN2,
        AIN3,
    };

    uint8_t config_reg[2];

    ads1015_dev->config.mux_config = adc_single_ended[channel];
    ads1015_dev->config.device_mode = SINGLE_SHOT;

    ads1015_read_reg(ads1015_dev, CONFIG_REGISTER, config_reg, sizeof(config_reg));
    config_reg[0] &= 0x0E;
    config_reg[0] |= (ads1015_dev->config.mux_config << 4) | (START_SINGLE_CONV << 7) | ads1015_dev->config.device_mode;

    ads1015_write_reg(ads1015_dev, CONFIG_REGISTER, config_reg, sizeof(config_reg));

    do
    {
        ads1015_read_reg(ads1015_dev, CONFIG_REGISTER, config_reg, sizeof(config_reg));
    }
    while ((config_reg[0] & 0x80) == CONV_IN_PROGRESS);

    uint8_t adc_data[2];

    ads1015_read_reg(ads1015_dev, CONVERSION_REGISTER, adc_data, sizeof(adc_data));

    int16_t result = ((adc_data[0] << 8) | adc_data[1]) >> 4;

    if (result & 0x0800)
    {
        result |= 0xF000;
    }
    return result;
}

float ads1015_convert_raw_value_to_voltage(ads1015_type_t* ads1015_dev, int16_t raw_value)
{
    const float lsb_mv[] = {
        0.003f,    // ±6.144V
        0.002f,    // ±4.096V
        0.001f,    // ±2.048V
        0.0005f,   // ±1.024V
        0.00025f,  // ±0.512V
        0.000125f, // ±0.256V
    };

    if (ads1015_dev->config.gain > 5)
        ads1015_dev->config.gain = 5;
    return raw_value * lsb_mv[ads1015_dev->config.gain];
}

float ads1015_read_channel_single_shot(ads1015_type_t* ads1015_dev, ads1015_adc_channel_t channel)
{
    return ads1015_convert_raw_value_to_voltage(
        ads1015_dev, ads1015_read_channel_raw_single_shot(ads1015_dev, channel));
}