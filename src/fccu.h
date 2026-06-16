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
#define PURGE_DURATION_MS 300

/** @brief Number of past samples compared for threshold-based purge detection. */
#define PURGE_COMPARE_SAMPLES 180

/** @brief Fuel cell voltage drop (V) that triggers a threshold-mode purge. */
#define PURGE_THRESHOLD_FC_DROP_V 5.0f

/** @brief Fan proportional controller target temperature in degrees Celsius. */
#define FAN_TARGET_C 50.0f

/** @brief Temperature deadband around FAN_TARGET_C where fan duty is clamped to minimum. */
#define FAN_DEADBAND_C 1.0f

/** @brief Temperature above FAN_TARGET_C at which fan duty reaches 100 %. */
#define FAN_FULLSCALE_C 10.0f

/** @brief Minimum fan duty cycle percentage when the fan is running. */
#define FAN_MIN_DUTY_PCT 20

/** @brief Default periodic purge interval in seconds. */
#define PURGE_PERIODIC_INTERVAL_S 60

/** @brief Number of samples averaged for HO-10P zero-current calibration. */
#define HO_ZERO_CAL_SAMPLES 100

/** @brief HO-10P current sensor conversion factor: amperes per volt. */
#define HO_I_PER_V (-12.2449f)

/** @brief Flowmeter pulse-to-volume factor: normalised litres per pulse. */
#define FLOW_LN_PER_PULSE 0.01f

/** @brief Moving average window size used for all sensor smoothing. */
#define MOV_AVG_SIZE 20

/** @brief NTC divider supply voltage (V). */
#define NTC_VCC_V 3.3f

/** @brief NTC divider fixed resistor (Ω). */
#define NTC_R_FIXED_OHM 4700.0f

/** @brief NTC nominal resistance at 25 °C (Ω). */
#define NTC_R0_OHM 1000.0f

/** @brief NTC β coefficient (K). */
#define NTC_BETA_K 3950.0f

/** @brief NTC reference temperature for R₀ (K). */
#define NTC_T0_K 298.15f

#define FCCU_CAN_FAST_PERIOD_MS 100
#define FCCU_CAN_STATUS_PERIOD_S 1
#define FCCU_CAN_ADS_PERIOD_S 5

/** @brief Maximum age of MCU CAN fc_v before threshold purge is skipped (ms). */
#define FC_V_CAN_STALE_MS 5000

/**
 * @brief Source for fuel cell voltage used in threshold purge and the log buffer.
 */
typedef enum {
    FC_V_SOURCE_ADC, /**< Local ESP32 ADC (zephyr,user ch0). */
    FC_V_SOURCE_CAN, /**< MCU_ANALOG_FUEL_CELL from CAN (mcu_data.fc_v). */
} fccu_fc_v_source_t;

/**
 * @brief Single ADC channel with its raw count and converted voltage.
 */
typedef struct {
    struct adc_dt_spec adc_channel; /**< Zephyr DT ADC channel spec. */
    int16_t            raw_value;   /**< Most recent raw ADC count. */
    float              voltage;     /**< Converted and averaged voltage (V). */
} fccu_adc_device_t;

/**
 * @brief Aggregated native ESP32 ADC measurements.
 */
typedef struct {
    fccu_adc_device_t fuel_cell_voltage; /**< Fuel cell stack voltage channel. */
    fccu_adc_device_t supercap_voltage;  /**< Supercapacitor voltage channel. */
    fccu_adc_device_t temp_sensor;       /**< NTC temperature channel on GPIO10 (ADC1 ch9). */
    float             temp_c;            /**< Averaged NTC temperature (°C). */
} fccu_adc_t;

/**
 * @brief Processed data from both ADS1015 ICs.
 */
typedef struct {
    float ads48[4];      /**< Raw ADS1015@48 channel voltages (V). */
    float ads49[4];      /**< Raw ADS1015@49 channel voltages (V). */
    int16_t ads48_raw[4]; /**< Raw ADS1015@48 ADC counts. */
    int16_t ads49_raw[4]; /**< Raw ADS1015@49 ADC counts. */
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
    const struct device *sensor;             /**< Zephyr sensor device handle. */
    struct sensor_value  temperature_buffer; /**< Raw Zephyr temperature value. */
    struct sensor_value  pressure_buffer;    /**< Raw Zephyr pressure value. */
    struct sensor_value  humidity_buffer;    /**< Raw Zephyr humidity value. */
    float                temperature;        /**< Averaged temperature (°C). */
    float                pressure;           /**< Averaged pressure (hPa × 10). */
    float                humidity;           /**< Averaged relative humidity (%). */
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
    bool main_valve_on;     /**< True when the main hydrogen valve is open. */
    bool purge_valve_on;    /**< True while a purge valve pulse is active. */
    bool fan_on;            /**< True when the fan enable GPIO is asserted. */
    bool measurements_tick; /**< Set by the 1 Hz k_timer; cleared after processing. */
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

extern float g_purge_threshold_v; /**< FC voltage drop threshold for threshold-mode purge (V). */
extern fccu_fc_v_source_t g_fc_v_source; /**< FC voltage source for threshold purge. */

/**
 * @brief Return true when the configured fc_v source has fresh data.
 */
bool fccu_fc_v_valid();

/**
 * @brief Get fuel cell voltage from the configured purge/log source.
 */
float fccu_fc_v_get();

/**
 * @brief Human-readable name for an fc_v source selector.
 */
const char *fccu_fc_v_source_name(fccu_fc_v_source_t src);

/**
 * @brief Initialise all FCCU subsystems.
 *
 * Must be called once from main() before the control loop starts. Initialises
 * ADC, ADS1015, CAN, valves, buttons, fan, flowmeter, and hardware counters
 * in the correct dependency order.
 */
void fccu_init();

/**
 * @brief Read sensors, update fan, log sample, and send slow CAN telemetry.
 *
 * Called from the 1 Hz system work item and from the hardware counter fallback.
 */
void fccu_process_measurements();

/**
 * @brief Execute one iteration of the main FCCU control loop.
 *
 * Handles fast CAN telemetry and the counter-based measurement fallback.
 * Periodic sensor reads run on the system workqueue via measurements_work.
 */
void fccu_on_tick();

void fccu_can_send_state();

/**
 * @brief Start the fuel cell system: open main valve, enable periodic purge.
 */
void fccu_start();

/**
 * @brief Stop the fuel cell system: close main valve, purge manual only.
 */
void fccu_stop();

#endif /* FCCU_H */
