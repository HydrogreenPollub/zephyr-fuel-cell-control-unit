#include "fccu_v2.h"

LOG_MODULE_REGISTER(fccu);

uint8_t fan_pwm_percent = 0;
static float last_fuel_cell_voltage = 0.0f;

volatile fccu_flags_t flags;
volatile fccu_state_t state = STOPPED;

ads1015_type_t ads1015_device;
ads1015_adc_data_t ads1015_data;

 fccu_valve_pin_t valve_pin = {
    .main_valve_on_pin = GPIO_DT_SPEC_GET(DT_ALIAS(main_valve_pin), gpios),
    .purge_valve_on_pin = GPIO_DT_SPEC_GET(DT_ALIAS(purge_valve_pin), gpios),
};

fccu_adc_t adc = {
    .low_pressure_sensor.adc_channel = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0),
    .fuel_cell_voltage.adc_channel = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1),
    .supercap_voltage.adc_channel = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 2),
    .temp_sensor.adc_channel = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 3),
};

 fccu_fan_t fan = {
    .fan_on_pin = GPIO_DT_SPEC_GET(DT_ALIAS(fan_pin), gpios),
    .fan_pwm = PWM_DT_SPEC_GET(DT_ALIAS(fan_pwm)),
};

 fccu_can_t can = {
    .can_device = DEVICE_DT_GET(DT_ALIAS(can)),
    .can_status_led = GPIO_DT_SPEC_GET(DT_ALIAS(can_led), gpios),
};

fccu_current_driver_t current_driver = {
    .driver_enable_pin = GPIO_DT_SPEC_GET(DT_ALIAS(driver_enable), gpios),
    .driver_pwm = PWM_DT_SPEC_GET(DT_ALIAS(driver_pwm)),
};

fccu_button_t button = {
    .button = GPIO_DT_SPEC_GET(DT_ALIAS(button_start), gpios),
    .button_external = GPIO_DT_SPEC_GET(DT_ALIAS(button_start_external), gpios),
};

bmp280_sensor_t sensor = {
    .sensor = DEVICE_DT_GET_ANY(bosch_bme280),
};

fccu_counter_t counter = {
    .counter_measurements = DEVICE_DT_GET(DT_ALIAS(counter0)),
    .counter_fuel_cell_voltage_check = DEVICE_DT_GET(DT_ALIAS(counter1)),
    .counter2 = DEVICE_DT_GET(DT_ALIAS(counter2)),
    .counter3 = DEVICE_DT_GET(DT_ALIAS(counter3)),
};

static struct k_work_delayable purge_valve_off_work;

void fccu_purge_valve_off(struct k_work *work)
{
    ARG_UNUSED(work);
    gpio_reset(&valve_pin.purge_valve_on_pin);
    flags.purge_valve_on = false;
    LOG_INF("Purge valve off\n");
}

static void fccu_purge_valve_on() {
    gpio_set(&valve_pin.purge_valve_on_pin);
    flags.purge_valve_on = true;
    LOG_INF("Purge Valve on\n");
    k_work_reschedule(&purge_valve_off_work, K_MSEC(PURGE_DURATION_MS));
}

void fccu_valves_init() {
    gpio_init(&valve_pin.main_valve_on_pin, GPIO_OUTPUT_INACTIVE);
    gpio_init(&valve_pin.purge_valve_on_pin, GPIO_OUTPUT_INACTIVE);
    k_work_init_delayable(&purge_valve_off_work, fccu_purge_valve_off);
    flags.main_valve_on = false;
    flags.purge_valve_on = false;
}

static void fccu_main_valve_on() {
    gpio_set(&valve_pin.main_valve_on_pin);
    flags.main_valve_on = true;
    LOG_INF("Main Valve on\n");
}

static void fccu_fan_on() {
    gpio_set(&fan.fan_on_pin);
    flags.fan_on = true;
    LOG_INF("Fan on\n");
}

static void fccu_current_driver_enable() {
    gpio_set(&current_driver.driver_enable_pin);
    LOG_INF("Current driver start\n");
}

static void fccu_fan_pwm_set(uint8_t pwm_percent) {
    pwm_set_pulse_width_percent(&fan.fan_pwm, pwm_percent);
    LOG_INF("Fan PWM set: %" PRIu8 "\n", pwm_percent);
}

static void fccu_current_driver_pwm_set(uint8_t pwm_percent) {
    pwm_set_pulse_width_percent(&current_driver.driver_pwm, pwm_percent);
    LOG_INF("Current driver PWM set: %" PRIu8 "\n", pwm_percent);
}

void fccu_adc_init() {
    adc_init(&adc.low_pressure_sensor.adc_channel);
    adc_init(&adc.fuel_cell_voltage.adc_channel);
    adc_init(&adc.supercap_voltage.adc_channel);
    adc_init(&adc.temp_sensor.adc_channel);
}

void fccu_fan_init() {
    gpio_init(&fan.fan_on_pin, GPIO_OUTPUT_INACTIVE);
    pwm_init(&fan.fan_pwm);
    flags.fan_on = false;
}

void fccu_can_init() {
    gpio_init(&can.can_status_led, GPIO_OUTPUT_INACTIVE);
    can_init(can.can_device, 500000);
}

void fccu_current_driver_init() {

    gpio_init(&current_driver.driver_enable_pin, GPIO_OUTPUT_INACTIVE);
    pwm_init(&current_driver.driver_pwm);
}

void fccu_counters_init() {
    counter_init(counter.counter_measurements);
    counter_init(counter.counter_fuel_cell_voltage_check);
    counter_init(counter.counter2);
    counter_init(counter.counter3);
}

static void counter_alarm_callback_measurements(const struct device *dev, void *user_data)
{
    // if (flags.start_button_pressed == true) {
    if (state == RUNNING){
        flags.measurements_tick = true;
        LOG_INF("Counter0 alarm triggered! \n");
    }
}

static void counter_alarm_callback_fuel_cell_voltage_check(const struct device *dev, void *user_data)
{
    if (state == RUNNING) {
        flags.compare_fuel_cell_voltage = true;
        LOG_INF("Counter1 alarm triggered! \r\n");
    }
}

// static void counter_alarm_callback2(const struct device *dev, void *user_data)
// {
//     LOG_INF("Counter2 alarm triggered!\n");
// }
//
// static void counter_alarm_callback3(const struct device *dev, void *user_data)
// {
//     LOG_INF("Counter3 alarm triggered!\n");
// }


void fccu_counters_set_interrupts() {
    counter_set_alarm(counter.counter_measurements, 0, counter_alarm_callback_measurements, 1000000);
    counter_set_alarm(counter.counter_fuel_cell_voltage_check, 0, counter_alarm_callback_fuel_cell_voltage_check, 30000000);
    // counter_set_alarm(counter.counter2, 0, counter_alarm_callback2, 3000000);
    // counter_set_alarm(counter.counter3, 0, counter_alarm_callback3, 4000000);
}

static void cooldown_expired(struct k_work *work)
{
    ARG_UNUSED(work);

    if (gpio_pin_get_dt(&button.button) == 0) {
        flags.start_button_pressed = true;
        printf("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
    }

}
static K_WORK_DELAYABLE_DEFINE(cooldown_work, cooldown_expired);

void button_pressed(const struct device *dev, struct gpio_callback *cb,
            uint32_t pins)
{
    k_work_reschedule(&cooldown_work, K_MSEC(1000));
}

static void cooldown_expired1(struct k_work *work)
{
    ARG_UNUSED(work);

    if (gpio_pin_get_dt(&button.button_external) == 1) {
        flags.start_button_pressed = true;
        printf("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
    }

}
static K_WORK_DELAYABLE_DEFINE(cooldown_work1, cooldown_expired1);

void button_pressed1(const struct device *dev, struct gpio_callback *cb,
            uint32_t pins)
{
    k_work_reschedule(&cooldown_work1, K_MSEC(1000));
}

void fccu_start_button_init() {
    gpio_init(&button.button, GPIO_INPUT);
    gpio_set_interrupt(&button.button, GPIO_INT_EDGE_TO_ACTIVE, &button.button_cb_data, button_pressed);

    gpio_init(&button.button_external, GPIO_INPUT);
    gpio_set_interrupt(&button.button_external, GPIO_INT_EDGE_TO_ACTIVE, &button.button_ext_cb_data, button_pressed1);
    flags.start_button_pressed = false;
}

void fccu_bmp280_sensor_init() {

    if (sensor.sensor == NULL) {
        LOG_ERR("\nError: no device found.\n");
        return;
    }

    if (!device_is_ready(sensor.sensor)) {
        LOG_ERR("\nError: Device \"%s\" is not ready\n", sensor.sensor->name);
        return;
    }

    LOG_INF("Found device \"%s\", getting sensor data\n", sensor.sensor->name);
}

void fccu_bmp280_sensor_read() {
    int ret = sensor_sample_fetch(sensor.sensor);
    if (ret) {
        LOG_ERR("Sensor sample update error: %d\n", ret);
        return;
    }
    sensor_channel_get(sensor.sensor, SENSOR_CHAN_AMBIENT_TEMP, &sensor.temperature_buffer);
    sensor_channel_get(sensor.sensor, SENSOR_CHAN_PRESS, &sensor.pressure_buffer);
    sensor_channel_get(sensor.sensor, SENSOR_CHAN_HUMIDITY, &sensor.humidity_buffer);

    sensor.temperature = sensor_value_to_float(&sensor.temperature_buffer);
    sensor.pressure = sensor_value_to_float(&sensor.pressure_buffer) * 10.0f;
    sensor.humidity = sensor_value_to_float(&sensor.humidity_buffer);

    LOG_INF("Temperature: %.2f C, Pressure: %.2f hPa, Humidity: %.2f RH\n", (double)sensor.temperature, (double)sensor.pressure, (double)sensor.humidity);
}

void fccu_init() {
    flags.measurements_tick = false;
    flags.compare_fuel_cell_voltage = false;
    flags.purge_valve_on = false;
    fccu_adc_init();
    ads1015_init(&ads1015_device);
    // // fccu_can_init();
    fccu_valves_init();
    fccu_fan_init();
    fccu_counters_init();
    fccu_start_button_init();
    fccu_counters_set_interrupts();
    fccu_bmp280_sensor_init();
    // fccu_current_driver_init();
    // fccu_current_driver_enable();
}

void fccu_adc_read() {
    adc_read_(&adc.low_pressure_sensor.adc_channel, &adc.low_pressure_sensor.raw_value);
    adc_read_(&adc.fuel_cell_voltage.adc_channel, &adc.fuel_cell_voltage.raw_value);
    adc_read_(&adc.supercap_voltage.adc_channel, &adc.supercap_voltage.raw_value);
    adc.supercap_voltage.voltage = adc_map((float)adc.supercap_voltage.raw_value, 0.0f, 2862.0f, 0.5f, 51.0f);
    adc_read_(&adc.temp_sensor.adc_channel, &adc.temp_sensor.raw_value);
    int32_t val = (int32_t)adc.temp_sensor.raw_value;
    adc_raw_to_millivolts_dt(&adc.temp_sensor.adc_channel, &val);
    adc.temp_sensor.voltage = (float)val / 1000.0f;
    float Vcc = 3.3f;
    float R = 10000.0f;
    float R_ntc = (R *  (Vcc - adc.temp_sensor.voltage)) / adc.temp_sensor.voltage;

    float T0 = 298.15f; // 25°C w kelwinach
    float R0 = 1000.0f; // 1kΩ przy 25°C
    float B = 3450.0f;

    float tempK = 1.0f / ((1.0f / T0) + (1.0f / B) * logf(R_ntc / R0));
    float tempC = tempK - 273.15f;
    LOG_INF("Low pressure sensor: %d, Fuel cell voltage: %d, Supercap voltage: %.3f, Temperature: %.3f\n", adc.low_pressure_sensor.raw_value, adc.fuel_cell_voltage.raw_value, \
      adc.supercap_voltage.voltage, tempC);
}

void fccu_ads1015_read() {
    ads1015_data.fuel_cell_current = ads1015_read_channel_single_shot(&ads1015_device, 0);
    ads1015_data.fuel_cell_current = adc_map(ads1015_data.fuel_cell_current, 1.508f, 1.432f, 0, 5); // Current sensor: 0-25A
    ads1015_data.high_pressure_sensor = ads1015_read_channel_single_shot(&ads1015_device, 2);
    ads1015_data.low_pressure_sensor = ads1015_read_channel_single_shot(&ads1015_device, 3);
    LOG_INF("Fuel cell current: %.2f A, High_pressure: %.2f, Low_pressure: %.2f\n", ads1015_data.fuel_cell_current, ads1015_data.high_pressure_sensor, \
        ads1015_data.low_pressure_sensor);
}

void fccu_on_tick() {


    if (flags.start_button_pressed == true){
        state = RUNNING;
        if (flags.measurements_tick == true) {
            fccu_bmp280_sensor_read();
            fccu_adc_read();
            fccu_ads1015_read();
            flags.measurements_tick = false;
        }
        if (!flags.main_valve_on) {
            fccu_main_valve_on();
        }
        if (!flags.fan_on) {
            fccu_fan_on();
            fan_pwm_percent = 20;
            fccu_fan_pwm_set(fan_pwm_percent);
        }
        if (flags.compare_fuel_cell_voltage && (!flags.purge_valve_on)) {
            if (last_fuel_cell_voltage - adc.supercap_voltage.voltage >= FC_V_PURGE_TRIGGER_DIFFERENCE) {
                fccu_purge_valve_on();
            }
            flags.compare_fuel_cell_voltage = false;
            last_fuel_cell_voltage = adc.supercap_voltage.voltage;
        }

        if (sensor.temperature >= FC_V_MAX_TEMPERATURE) {
            fan_pwm_percent = 100;
            fccu_fan_pwm_set(fan_pwm_percent);
        }

    }
    // int8_t current_driver_pwm = 10;


    // fccu_current_driver_set_pwm(&fccu->current_driver, current_driver_pwm);
    // // fccu_fan_pwm_set(&fccu->fan, current_driver_pwm);
    // fccu->ads1015_data.fuel_cell_current = ads1015_read_channel_single_shot(&fccu->ads1015_device, 0);
    // fccu->ads1015_data.fuel_cell_current = adc_map(fccu->ads1015_data.fuel_cell_current, 1.508f, 1.432, 0, 5); // Current sensor: 0-25A
    // LOG_INF("Current sensor = %.3f A, PWM = %d\r\n",(double)fccu->ads1015_data.fuel_cell_current, current_driver_pwm);


    // if (fccu->ads1015_data.fuel_cell_current > FC_MAX_CURRENT) {
    //     current_driver_pwm -= 20;
    //     if (current_driver_pwm < 0)
    //         current_driver_pwm = 0;
    //
    //     fccu_current_driver_set_pwm(&fccu->current_driver, current_driver_pwm);
    //
    // }
    // else if (fccu->ads1015_data.fuel_cell_current < FC_MIN_CURRENT) {
    //     current_driver_pwm += 5;
    //     if (current_driver_pwm > 100)
    //         current_driver_pwm = 100;
    //
    //     fccu_current_driver_set_pwm(&fccu->current_driver, current_driver_pwm);
    //
    // }
    k_msleep(100);
}

