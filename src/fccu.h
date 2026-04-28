#ifndef FCCU_H
#define FCCU_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/can.h>
#include "gpio.h"
#include "pwm.h"
#include "adc.h"
#include "status_led.h"

/** @brief Duration of a single purge valve pulse in milliseconds. */
#define PURGE_DURATION_MS           300

/** @brief Number of past samples compared for threshold-based purge detection. */
#define PURGE_COMPARE_SAMPLES       180

/** @brief Fuel cell voltage drop (V) that triggers a threshold-mode purge. */
#define PURGE_TRIGGER_FC_DROP_V     5.0f

/** @brief Fan proportional controller target temperature in degrees Celsius. */
#define FAN_TARGET_C                50.0f

/** @brief Temperature deadband around FAN_TARGET_C where fan duty is clamped to minimum. */
#define FAN_DEADBAND_C              1.0f

/** @brief Temperature above FAN_TARGET_C at which fan duty reaches 100 %. */
#define FAN_FULLSCALE_C             10.0f

/** @brief Minimum fan duty cycle percentage when the fan is running. */
#define FAN_MIN_DUTY_PCT            20

/** @brief Default periodic purge interval in seconds. */
#define PURGE_PERIODIC_INTERVAL_S   60

/** @brief Number of samples averaged for HO-10P zero-current calibration. */
#define HO_ZERO_CAL_SAMPLES         100

/** @brief HO-10P current sensor conversion factor: amperes per volt. */
#define HO_I_PER_V                  (-12.2449f)

/** @brief Flowmeter pulse-to-volume factor: normalised litres per pulse. */
#define FLOW_LN_PER_PULSE           0.01f

/** @brief Moving average window size used for all sensor smoothing. */
#define MOV_AVG_SIZE                20

/**
 * @brief Single ADC channel with its raw count and converted voltage.
 */
typedef struct {
    struct adc_dt_spec adc_channel; /**< Zephyr DT ADC channel spec. */
    int16_t raw_value;              /**< Most recent raw ADC count. */
    float   voltage;                /**< Converted and averaged voltage (V). */
} fccu_adc_device_t;

/**
 * @brief Aggregated native ESP32 ADC measurements.
 */
typedef struct {
    fccu_adc_device_t fuel_cell_voltage; /**< Fuel cell stack voltage channel. */
    fccu_adc_device_t supercap_voltage;  /**< Supercapacitor voltage channel. */
    fccu_adc_device_t temp_sensor;       /**< Onboard NTC temperature channel. */
} fccu_adc_t;

/**
 * @brief Processed data from both ADS1015 ICs.
 */
typedef struct {
    float ads48[4];      /**< Raw ADS1015@48 channel voltages (V). */
    float ads49[4];      /**< Raw ADS1015@49 channel voltages (V). */
    float fc_current;    /**< Fuel cell current derived from ads48[0] (A). */
    float fc_temp_c;     /**< Fuel cell NTC temperature from ads48[1] (°C). */
    float hp_sensor;     /**< High-pressure sensor voltage from ads48[2] (V). */
    float lp_sensor;     /**< Low-pressure sensor voltage from ads48[3] (V). */
    float ho_current[4]; /**< HO-10P hall-effect currents from ads49 channels (A). */
} ads1015_adc_data_t;

/**
 * @brief Fan hardware resources.
 */
typedef struct {
    struct pwm_dt_spec  fan_pwm;    /**< PWM output for speed control. */
    struct gpio_dt_spec fan_on_pin; /**< GPIO that enables the fan driver IC. */
} fccu_fan_t;

/**
 * @brief Hydrogen valve GPIO pins.
 */
typedef struct {
    struct gpio_dt_spec main_valve_on_pin;  /**< Main hydrogen supply valve. */
    struct gpio_dt_spec purge_valve_on_pin; /**< Purge / exhaust valve. */
} fccu_valve_pin_t;

/**
 * @brief CAN bus hardware resources and status LED.
 */
typedef struct {
    const struct device *can_device;     /**< Zephyr CAN device handle. */
    struct gpio_dt_spec  can_status_led; /**< LED that mirrors CAN bus state. */
} fccu_can_t;

/**
 * @brief BME280 environmental sensor state and averaged readings.
 */
typedef struct {
    const struct device    *sensor;             /**< Zephyr sensor device handle. */
    struct sensor_value     temperature_buffer; /**< Raw Zephyr temperature value. */
    struct sensor_value     pressure_buffer;    /**< Raw Zephyr pressure value. */
    struct sensor_value     humidity_buffer;    /**< Raw Zephyr humidity value. */
    float temperature; /**< Averaged temperature (°C). */
    float pressure;    /**< Averaged pressure (hPa × 10). */
    float humidity;    /**< Averaged relative humidity (%). */
} bmp280_sensor_t;

/**
 * @brief Counter device handles used for periodic hardware alarms.
 */
typedef struct {
    const struct device *counter_measurements;            /**< 1 Hz measurement tick counter. */
    const struct device *counter_fuel_cell_voltage_check; /**< Reserved voltage-check counter. */
    const struct device *counter2;                        /**< Reserved counter 2. */
    const struct device *counter3;                        /**< Reserved counter 3. */
} fccu_counter_t;

/**
 * @brief Global state flags shared across all FCCU modules.
 */
typedef struct {
    bool main_valve_on;    /**< True when the main hydrogen valve is open. */
    bool purge_valve_on;   /**< True while a purge valve pulse is active. */
    bool fan_on;           /**< True when the fan enable GPIO is asserted. */
    bool measurements_tick; /**< Set by the 1 Hz counter ISR; cleared after processing. */
} fccu_flags_t;

/**
 * @brief Current driver hardware resources (reserved for future use).
 */
typedef struct {
    struct pwm_dt_spec  driver_pwm;        /**< PWM output for the current driver. */
    struct gpio_dt_spec driver_enable_pin; /**< Enable GPIO for the current driver. */
} fccu_current_driver_t;

/**
 * @brief Top-level fuel cell operating states.
 */
typedef enum {
    RUNNING,    /**< Main valve open; automatic purge active. */
    STOPPED,    /**< Main valve closed; purge in manual mode. */
    IDLE_STATE, /**< Transitional / unused state. */
} fccu_state_t;

extern volatile fccu_flags_t flags; /**< Global FCCU state flags. */
extern volatile fccu_state_t state; /**< Current operating state of the fuel cell. */
extern fccu_can_t            can;   /**< CAN bus device and status LED. */

extern float g_purge_trigger_v; /**< FC voltage drop threshold for threshold-mode purge (V). */

/**
 * @brief Initialise all FCCU subsystems.
 *
 * Must be called once from main() before the control loop starts. Initialises
 * ADC, ADS1015, CAN, valves, buttons, fan, flowmeter, and hardware counters
 * in the correct dependency order.
 */
void fccu_init();

/**
 * @brief Execute one iteration of the main FCCU control loop.
 *
 * When the 1 Hz measurements_tick flag is set, reads all sensors, updates fan
 * duty, evaluates purge logic, appends a log sample, and sends telemetry CAN
 * frames. Sleeps 100 ms at the end of each call to yield the CPU to other
 * Zephyr threads.
 */
void fccu_on_tick();

#endif /* FCCU_H */
