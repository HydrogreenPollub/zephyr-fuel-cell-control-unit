#ifndef FCCU_V2_H
#define FCCU_V2_H


#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor_data_types.h>
#include <zephyr/logging/log.h>
#include "ads1015.h"
#include "counter.h"
#include "can.h"
#include "gpio.h"
#include "pwm.h"
#include "adc.h"
#include <math.h>

#define FC_MAX_CURRENT 1.1f
#define FC_MIN_CURRENT 0.9f
#define PURGE_DURATION_MS             300
#define FC_V_PURGE_TRIGGER_DIFFERENCE 2.0f
#define FC_V_MAX_TEMPERATURE 65.0f

typedef struct {
    struct adc_dt_spec adc_channel;
    int16_t raw_value;
    float voltage;
}fccu_adc_device_t;

typedef struct {
    fccu_adc_device_t low_pressure_sensor;
    fccu_adc_device_t fuel_cell_voltage;
    fccu_adc_device_t supercap_voltage;
    fccu_adc_device_t temp_sensor;
}fccu_adc_t;

typedef struct {
    float low_pressure_sensor;
    float high_pressure_sensor;
    float fuel_cell_current;
    float fuel_cell_voltage;
}ads1015_adc_data_t;

typedef struct {
    struct pwm_dt_spec fan_pwm;
    struct gpio_dt_spec fan_on_pin;
}fccu_fan_t;

typedef struct {
    struct gpio_dt_spec main_valve_on_pin;
    struct gpio_dt_spec purge_valve_on_pin;
}fccu_valve_pin_t;

typedef struct {
    const struct device *can_device;
    struct gpio_dt_spec can_status_led;
}fccu_can_t;

typedef struct {
    const struct device *sensor;
    struct sensor_value temperature_buffer;
    struct sensor_value pressure_buffer;
    struct sensor_value humidity_buffer;
    float temperature;
    float pressure;
    float humidity;
}bmp280_sensor_t;

typedef struct {
    struct gpio_dt_spec button;
    struct gpio_dt_spec button_external;
    struct gpio_callback button_cb_data;
    struct gpio_callback button_ext_cb_data;
    struct k_work_delayable work;
}fccu_button_t;

typedef struct {
    const struct device *counter_measurements;
    const struct device *counter_fuel_cell_voltage_check;
    const struct device *counter2;
    const struct device *counter3;
}fccu_counter_t;

typedef struct {
    bool start_button_pressed;
    bool main_valve_on;
    bool purge_valve_on;
    bool fan_on;
    bool measurements_tick;
    bool compare_fuel_cell_voltage;
}fccu_flags_t;

typedef struct {
    struct pwm_dt_spec driver_pwm;
    struct gpio_dt_spec driver_enable_pin;
}fccu_current_driver_t;

typedef enum {
    RUNNING,
    STOPPED,
    IDLE_STATE,
}fccu_state_t;

// typedef struct {
//     fccu_valve_pin_t valve_pins;
//     fccu_button_t start_button;
//     fccu_fan_t fan;
//     fccu_adc_t adc;
//     fccu_can_t can;
//     fccu_counter_t counter;
//     bmp280_sensor_t bmp280_sensor;
//     ads1015_adc_data_t ads1015_data;
//     ads1015_type_t ads1015_device;
//     fccu_current_driver_t current_driver;
// }fccu_device_t;

extern volatile fccu_flags_t flags;
extern volatile fccu_state_t state;
extern fccu_valve_pin_t valve_pin;
extern fccu_button_t button;
extern bmp280_sensor_t sensor;
extern fccu_current_driver_t current_driver;
extern fccu_can_t can;
extern fccu_fan_t fan;
extern fccu_adc_t adc;

void fccu_init();
void fccu_adc_init();
void fccu_can_init();
void fccu_fan_init();
void fccu_valves_init();
void fccu_start_button_init();
void fccu_bmp280_sensor_init();
void fccu_current_driver_init();
void fccu_ads1015_read();
void fccu_counters_init();
void fccu_counters_set_interrupts();

void button_pressed(const struct device *dev, struct gpio_callback *cb,
            uint32_t pins);
void fccu_bmp280_sensor_read();
void fccu_adc_read();

void fccu_on_tick();

#endif //FCCU_V2_H
