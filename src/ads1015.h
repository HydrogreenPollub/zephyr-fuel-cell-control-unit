#ifndef ADS1015_H
#define ADS1015_H

#include <stdint.h>
#include <zephyr/drivers/i2c.h>
#include <string.h>

/** @brief Default I2C address of the ADS1015 (ADDR pin tied to GND). */
#define ADS1015_I2C_ADDRESS 0x48

/**
 * @brief ADS1015 internal register address pointer values.
 */
typedef enum {
    CONVERSION_REGISTER  = 0x00, /**< Most recent conversion result. */
    CONFIG_REGISTER      = 0x01, /**< Device configuration and OS bit. */
    LOW_TRESH_REGISTER   = 0x02, /**< Comparator low-threshold register. */
    HIGH_TRESH_REGISTER  = 0x03, /**< Comparator high-threshold register. */
} ads1015_addr_ptr_reg_t;

/**
 * @brief Programmable gain amplifier (PGA) full-scale range settings.
 */
typedef enum {
    FSR_6_144V = 0x00, /**< ±6.144 V full-scale range. */
    FSR_4_096V = 0x01, /**< ±4.096 V full-scale range. */
    FSR_2_048V = 0x02, /**< ±2.048 V full-scale range (default). */
    FSR_1_024V = 0x03, /**< ±1.024 V full-scale range. */
    FSR_0_512V = 0x04, /**< ±0.512 V full-scale range. */
    FSR_0_256V = 0x05, /**< ±0.256 V full-scale range. */
} ads1015_prog_gain_amplifier_t;

/**
 * @brief ADC data rate (samples per second) settings.
 */
typedef enum {
    SPS_128  = 0x00, /**< 128 samples per second. */
    SPS_250  = 0x01, /**< 250 samples per second. */
    SPS_490  = 0x02, /**< 490 samples per second. */
    SPS_920  = 0x03, /**< 920 samples per second. */
    SPS_1600 = 0x04, /**< 1600 samples per second (default). */
    SPS_2400 = 0x05, /**< 2400 samples per second. */
    SPS_3300 = 0x06, /**< 3300 samples per second. */
} ads1015_data_rate_t;

/**
 * @brief Input multiplexer configuration (single-ended and differential pairs).
 */
typedef enum {
    AIN0_AIN1 = 0x00, /**< Differential: AIN0 − AIN1. */
    AIN0_AIN3 = 0x01, /**< Differential: AIN0 − AIN3. */
    AIN1_AIN3 = 0x02, /**< Differential: AIN1 − AIN3. */
    AIN2_AIN3 = 0x03, /**< Differential: AIN2 − AIN3. */
    AIN0      = 0x04, /**< Single-ended: AIN0 vs GND. */
    AIN1      = 0x05, /**< Single-ended: AIN1 vs GND. */
    AIN2      = 0x06, /**< Single-ended: AIN2 vs GND. */
    AIN3      = 0x07, /**< Single-ended: AIN3 vs GND. */
} ads1015_input_mux_config_t;

/**
 * @brief Logical ADC channel indices used by the high-level read API.
 */
typedef enum {
    ADC_CHANNEL_0, /**< Physical pin AIN0. */
    ADC_CHANNEL_1, /**< Physical pin AIN1. */
    ADC_CHANNEL_2, /**< Physical pin AIN2. */
    ADC_CHANNEL_3, /**< Physical pin AIN3. */
} ads1015_adc_channel_t;

/**
 * @brief Conversion mode selection.
 */
typedef enum {
    CONTINOUS_CONVERSION = 0x00, /**< Continuous-conversion mode. */
    SINGLE_SHOT          = 0x01, /**< Power-down after each conversion. */
} ads1015_device_mode_t;

/**
 * @brief Comparator output mode.
 */
typedef enum {
    TRADITIONAL = 0x00, /**< Traditional comparator with hysteresis. */
    WINDOW      = 0x01, /**< Window comparator. */
} ads1015_comp_mode_t;

/**
 * @brief Comparator output polarity.
 */
typedef enum {
    ACTIVE_LOW  = 0x00, /**< ALERT/RDY pin active low (default). */
    ACTIVE_HIGH = 0x01, /**< ALERT/RDY pin active high. */
} ads1015_comp_polarity_t;

/**
 * @brief Comparator output latching behaviour.
 */
typedef enum {
    NONLATCHING = 0x00, /**< Comparator output does not latch. */
    LATCHING    = 0x01, /**< Comparator output latches until cleared. */
} ads1015_comp_latching_t;

/**
 * @brief Number of consecutive out-of-range readings before ALERT/RDY fires.
 */
typedef enum {
    AFTER_1_CONV = 0x00, /**< Assert after one conversion. */
    AFTER_2_CONV = 0x01, /**< Assert after two conversions. */
    AFTER_4_CONV = 0x02, /**< Assert after four conversions. */
    DISABLE      = 0x03, /**< Disable the comparator (default). */
} ads1015_comp_queue_t;

/**
 * @brief OS (operational status) field written to the config register.
 */
typedef enum {
    IDLE              = 0x00, /**< No effect (device not idle). */
    START_SINGLE_CONV = 0x01, /**< Start a single-shot conversion. */
} operational_status_write_t;

/**
 * @brief OS field read back from the config register.
 */
typedef enum {
    CONV_IN_PROGRESS, /**< Device is currently converting. */
    CONV_DONE,        /**< Conversion is complete; result ready. */
} operational_status_read_t;

/**
 * @brief Comparator configuration bit-fields (maps to config register bits 0–4).
 */
typedef struct {
    ads1015_comp_mode_t      comp_mode     : 1; /**< Comparator mode. */
    ads1015_comp_polarity_t  comp_polarity : 1; /**< ALERT/RDY polarity. */
    ads1015_comp_latching_t  comp_latching : 1; /**< Latching behaviour. */
    ads1015_comp_queue_t     comp_queue    : 2; /**< Queue / disable setting. */
} ads1015_comp_t;

/**
 * @brief ADC configuration bit-fields (maps to config register bits 5–15).
 */
typedef struct {
    ads1015_input_mux_config_t    mux_config   : 3; /**< Input multiplexer selection. */
    ads1015_prog_gain_amplifier_t gain         : 3; /**< PGA full-scale range. */
    ads1015_device_mode_t         device_mode  : 1; /**< Continuous or single-shot. */
    ads1015_data_rate_t           data_rate    : 3; /**< Samples per second. */
} ads1015_config_t;

/**
 * @brief ADS1015 device handle holding I2C reference and current configuration.
 */
typedef struct {
    const struct i2c_dt_spec *i2c;   /**< Zephyr I2C devicetree spec for this device. */
    uint16_t        i2c_addr;        /**< I2C address (normally ADS1015_I2C_ADDRESS). */
    ads1015_config_t  config;        /**< Current ADC configuration. */
    ads1015_comp_t    comparator;    /**< Current comparator configuration. */
} ads1015_type_t;

/**
 * @brief Initialise an ADS1015 device with default configuration.
 *
 * Sets single-ended mode on AIN0, FSR_4_096V gain, single-shot mode,
 * 1600 SPS data rate, and disables the comparator. Writes the configuration
 * to the device over I2C.
 *
 * @param ads1015_dev Pointer to the device handle to initialise.
 */
void ads1015_init(ads1015_type_t *ads1015_dev);

/**
 * @brief Write a register on the ADS1015 over I2C.
 *
 * Prepends the register address byte to @p data and performs a single I2C
 * write transaction.
 *
 * @param ads1015_dev Pointer to the device handle.
 * @param reg         Register address (see ads1015_addr_ptr_reg_t).
 * @param data        Pointer to the data bytes to write.
 * @param data_len    Number of data bytes (excluding the register byte).
 */
void ads1015_write_reg(ads1015_type_t *ads1015_dev, uint8_t reg, uint8_t *data, size_t data_len);

/**
 * @brief Read a register from the ADS1015 over I2C.
 *
 * Performs a combined write (register address) / read (data) I2C transaction.
 *
 * @param ads1015_dev Pointer to the device handle.
 * @param reg         Register address (see ads1015_addr_ptr_reg_t).
 * @param data_read   Buffer to store the read bytes.
 * @param data_len    Number of bytes to read.
 */
void ads1015_read_reg(ads1015_type_t *ads1015_dev, uint8_t reg, uint8_t *data_read, size_t data_len);

/**
 * @brief Trigger a single-shot conversion and return the raw 12-bit result.
 *
 * Selects the requested single-ended channel, starts a conversion, then polls
 * the OS bit until the result is ready. Returns a sign-extended 12-bit value.
 *
 * @param ads1015_dev Pointer to the device handle.
 * @param channel     Channel to sample (ADC_CHANNEL_0 … ADC_CHANNEL_3).
 * @return Raw signed 12-bit ADC result.
 */
int16_t ads1015_read_channel_raw_single_shot(ads1015_type_t *ads1015_dev, ads1015_adc_channel_t channel);

/**
 * @brief Convert a raw 12-bit ADC result to a voltage using the current PGA setting.
 *
 * Uses the LSB size table derived from the configured gain to scale @p raw_value.
 *
 * @param ads1015_dev Pointer to the device handle (provides the gain setting).
 * @param raw_value   Signed 12-bit raw ADC result.
 * @return Voltage in volts.
 */
float ads1015_convert_raw_value_to_voltage(ads1015_type_t *ads1015_dev, int16_t raw_value);

/**
 * @brief Perform a single-shot conversion and return the result as a voltage.
 *
 * Convenience wrapper that calls ads1015_read_channel_raw_single_shot() and
 * ads1015_convert_raw_value_to_voltage() in sequence.
 *
 * @param ads1015_dev Pointer to the device handle.
 * @param channel     Channel to sample (ADC_CHANNEL_0 … ADC_CHANNEL_3).
 * @return Measured voltage in volts.
 */
float ads1015_read_channel_single_shot(ads1015_type_t *ads1015_dev, ads1015_adc_channel_t channel);

#endif /* ADS1015_H */
