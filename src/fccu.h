#ifndef FCCU_H
#define FCCU_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/can.h>
#include "gpio.h"
#include "pwm.h"
#include "adc.h"
#include "status_led.h"

/* Purge valve */
#define PURGE_DURATION_MS           300
#define PURGE_COMPARE_SAMPLES       180
#define PURGE_TRIGGER_FC_DROP_V     5.0f

/* Fan proportional control */
#define FAN_TARGET_C                50.0f
#define FAN_DEADBAND_C              1.0f
#define FAN_FULLSCALE_C             10.0f
#define FAN_MIN_DUTY_PCT            20

/* Main valve */
#define MAIN_VALVE_DEFAULT_PCT      50

/* HO-10P current sensor */
#define HO_ZERO_CAL_SAMPLES         100
#define HO_I_PER_V                  (-12.2449f)

/* Moving average */
#define MOV_AVG_SIZE                20

typedef struct {
    struct adc_dt_spec adc_channel;
    int16_t raw_value;
    float   voltage;
} fccu_adc_device_t;

typedef struct {
    fccu_adc_device_t fuel_cell_voltage;
    fccu_adc_device_t supercap_voltage;
    fccu_adc_device_t temp_sensor;
} fccu_adc_t;

typedef struct {
    float ads48[4];
    float ads49[4];
    float fc_current;
    float fc_temp_c;
    float hp_sensor;
    float lp_sensor;
    float ho_current[4];
} ads1015_adc_data_t;

typedef struct {
    struct pwm_dt_spec  fan_pwm;
    struct gpio_dt_spec fan_on_pin;
} fccu_fan_t;

typedef struct {
    struct gpio_dt_spec main_valve_on_pin;
    struct gpio_dt_spec purge_valve_on_pin;
} fccu_valve_pin_t;

typedef struct {
    const struct device *can_device;
    struct gpio_dt_spec  can_status_led;
} fccu_can_t;

typedef struct {
    const struct device    *sensor;
    struct sensor_value     temperature_buffer;
    struct sensor_value     pressure_buffer;
    struct sensor_value     humidity_buffer;
    float temperature;
    float pressure;
    float humidity;
} bmp280_sensor_t;

typedef struct {
    const struct device *counter_measurements;
    const struct device *counter_fuel_cell_voltage_check;
    const struct device *counter2;
    const struct device *counter3;
} fccu_counter_t;

typedef struct {
    bool main_valve_on;
    bool purge_valve_on;
    bool fan_on;
    bool measurements_tick;
} fccu_flags_t;

typedef struct {
    struct pwm_dt_spec  driver_pwm;
    struct gpio_dt_spec driver_enable_pin;
} fccu_current_driver_t;

typedef enum {
    RUNNING,
    STOPPED,
    IDLE_STATE,
} fccu_state_t;

extern volatile fccu_flags_t flags;
extern volatile fccu_state_t state;
extern fccu_can_t            can;

/* Configurable setpoints (shell-adjustable) */
extern float   g_purge_trigger_v;
extern uint8_t g_main_valve_setpoint_pct;

/* Initialise all subsystems. Call once before fccu_on_tick(). */
void fccu_init();

/* Process one iteration of the main control loop. Call in a tight while(1). */
void fccu_on_tick();

#endif /* FCCU_H */
