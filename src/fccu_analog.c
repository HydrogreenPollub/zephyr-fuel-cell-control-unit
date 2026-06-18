#include "fccu_analog.h"

#include <math.h>
#include <zephyr/init.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

#include "ads1015.h"
#include "can.h"
#include "candef.h"
#include "fccu_flow.h"

LOG_MODULE_REGISTER(fccu_analog, LOG_LEVEL_INF);

bool g_analog_log_enabled = false;

fccu_adc_t adc = {
    .fuel_cell_voltage.adc_channel = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0),
    .supercap_voltage.adc_channel  = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1),
    .temp_sensor.adc_channel       = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 2),
};

bmp280_sensor_t sensor = {
    .sensor = DEVICE_DT_GET(DT_NODELABEL(bme280_76)),
};

bmp280_sensor_t sensor2 = {
    .sensor = DEVICE_DT_GET(DT_NODELABEL(bme280_77)),
};

ads1015_adc_data_t ads1015_data;
float              ho_zero_v[4] = {0};

static const struct i2c_dt_spec ads48_i2c = I2C_DT_SPEC_GET(DT_NODELABEL(ads1015_48));
static const struct i2c_dt_spec ads49_i2c = I2C_DT_SPEC_GET(DT_NODELABEL(ads1015_49));

static ads1015_type_t ads1015_48_dev;
static ads1015_type_t ads1015_49_dev;
static bool           s_ads48_ok = false;
static bool           s_ads49_ok = false;

typedef struct {
    float buf[MOV_AVG_SIZE];
    int   idx;
    int   count;
    float sum;
} mov_avg_t;

static mov_avg_t avg_ads48[4];
static mov_avg_t avg_ads49[4];
static mov_avg_t avg_bme76_t, avg_bme76_h, avg_bme76_p;
static mov_avg_t avg_bme77_t, avg_bme77_h, avg_bme77_p;
static mov_avg_t avg_fc_v, avg_sc_v, avg_ntc_t;

static float mov_avg_push(mov_avg_t *m, float v)
{
    if (!isfinite(v)) {
        return (m->count > 0) ? (m->sum / (float)m->count) : 0.0f;
    }

    if (m->count < MOV_AVG_SIZE) {
        m->buf[m->idx] = v;
        m->sum += v;
        m->count++;
    } else {
        m->sum -= m->buf[m->idx];
        m->buf[m->idx] = v;
        m->sum += v;
    }
    m->idx = (m->idx + 1) % MOV_AVG_SIZE;
    return (m->count > 0) ? (m->sum / (float)m->count) : 0.0f;
}

static int fccu_i2c_early_recover()
{
    const struct device *bus = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    if (device_is_ready(bus)) {
        i2c_recover_bus(bus);
    }
    return 0;
}
SYS_INIT(fccu_i2c_early_recover, APPLICATION, 5);

float fccu_ntc_voltage_to_celsius(float adc_voltage)
{
    if (adc_voltage <= 0.0f || adc_voltage >= NTC_VCC_V) {
        return NAN;
    }

    double r_ntc =
        (double)NTC_R_FIXED_OHM * (double)adc_voltage / ((double)NTC_VCC_V - (double)adc_voltage);
    if (r_ntc <= 0.0) {
        return NAN;
    }

    double inv_t =
        (1.0 / (double)NTC_T0_K) + (1.0 / (double)NTC_BETA_K) * log(r_ntc / (double)NTC_R0_OHM);
    if (inv_t <= 0.0) {
        return NAN;
    }

    return (float)((1.0 / inv_t) - 273.15);
}

void fccu_adc_init()
{
    adc_init(&adc.fuel_cell_voltage.adc_channel);
    adc_init(&adc.supercap_voltage.adc_channel);
    adc_init(&adc.temp_sensor.adc_channel);
}

void fccu_adc_read()
{
    adc_read_(&adc.fuel_cell_voltage.adc_channel, &adc.fuel_cell_voltage.raw_value);
    adc.fuel_cell_voltage.voltage = mov_avg_push(
        &avg_fc_v, adc_map((float)adc.fuel_cell_voltage.raw_value, 1.0f, 2853.8f, 0.4f, 52.0f));

    adc_read_(&adc.supercap_voltage.adc_channel, &adc.supercap_voltage.raw_value);
    adc.supercap_voltage.voltage = mov_avg_push(
        &avg_sc_v, adc_map((float)adc.supercap_voltage.raw_value, 1.0f, 2853.8f, 0.4f, 52.0f));

    adc_read_(&adc.temp_sensor.adc_channel, &adc.temp_sensor.raw_value);
    int32_t val = (int32_t)adc.temp_sensor.raw_value;
    adc_raw_to_millivolts_dt(&adc.temp_sensor.adc_channel, &val);
    adc.temp_sensor.voltage = (float)val / 1000.0f;
    adc.temp_c = mov_avg_push(&avg_ntc_t, fccu_ntc_voltage_to_celsius(adc.temp_sensor.voltage));
}

void fccu_adc_can_send()
{
    struct candef_fccu_power_t power_msg = {
        .fc_voltage = candef_fccu_power_fc_voltage_encode((double)adc.fuel_cell_voltage.voltage),
        .fc_current = candef_fccu_power_fc_current_encode((double)ads1015_data.fc_current),
        .accessory_voltage =
            candef_fccu_power_accessory_voltage_encode((double)adc.supercap_voltage.voltage),
    };
    uint8_t power_buf[CANDEF_FCCU_POWER_LENGTH];
    candef_fccu_power_pack(power_buf, &power_msg, sizeof(power_buf));
    can_send_(can.can_device, CANDEF_FCCU_POWER_FRAME_ID, power_buf, sizeof(power_buf));
}

void fccu_hydrogen_can_send()
{
    float lp_bar = (ads1015_data.lp_sensor - 0.120f) / (2.805f - 0.120f);

    struct candef_fccu_hydrogen_t hydrogen_msg = {
        .h2_flow        = candef_fccu_hydrogen_h2_flow_encode((double)flow_rate_lnmin),
        .h2_lp_pressure = candef_fccu_hydrogen_h2_lp_pressure_encode((double)lp_bar),
        .h2_hp_pressure =
            candef_fccu_hydrogen_h2_hp_pressure_encode((double)ads1015_data.hp_sensor),
        .leakage_voltage =
            candef_fccu_hydrogen_leakage_voltage_encode((double)ads1015_data.ads48[3]),
    };
    uint8_t hydrogen_buf[CANDEF_FCCU_HYDROGEN_LENGTH];
    candef_fccu_hydrogen_pack(hydrogen_buf, &hydrogen_msg, sizeof(hydrogen_buf));
    can_send_(can.can_device, CANDEF_FCCU_HYDROGEN_FRAME_ID, hydrogen_buf, sizeof(hydrogen_buf));
}

static void fccu_ho_zero_calibrate()
{
    if (!s_ads49_ok) {
        return;
    }
    float sums[4] = {0};
    for (int n = 0; n < HO_ZERO_CAL_SAMPLES; n++) {
        for (int ch = 0; ch < 4; ch++) {
            sums[ch] += ads1015_read_channel_single_shot(&ads1015_49_dev, ch);
        }
        k_msleep(10);
    }
    for (int ch = 0; ch < 4; ch++) {
        ho_zero_v[ch] = sums[ch] / HO_ZERO_CAL_SAMPLES;
    }
    LOG_INF("HO zero cal: %.4f %.4f %.4f %.4f", (double)ho_zero_v[0], (double)ho_zero_v[1],
            (double)ho_zero_v[2], (double)ho_zero_v[3]);
}

void fccu_ads1015_init()
{
    uint8_t probe = 0;

    ads1015_48_dev.i2c = &ads48_i2c;
    if (i2c_write_dt(&ads48_i2c, &probe, 1) == 0) {
        s_ads48_ok = true;
        ads1015_init(&ads1015_48_dev);
        LOG_INF("ADS1015@48 ready");
    } else {
        LOG_WRN("ADS1015@48 not found");
    }

    ads1015_49_dev.i2c = &ads49_i2c;
    if (i2c_write_dt(&ads49_i2c, &probe, 1) == 0) {
        s_ads49_ok = true;
        ads1015_init(&ads1015_49_dev);
        LOG_INF("ADS1015@49 ready");
    } else {
        LOG_WRN("ADS1015@49 not found");
    }

    fccu_ho_zero_calibrate();
}

void fccu_ads1015_read()
{
    if (s_ads48_ok) {
        for (int ch = 0; ch < 4; ch++) {
            int16_t raw                = ads1015_read_channel_raw_single_shot(&ads1015_48_dev, ch);
            ads1015_data.ads48_raw[ch] = raw;
            ads1015_data.ads48[ch]     = mov_avg_push(
                &avg_ads48[ch], ads1015_convert_raw_value_to_voltage(&ads1015_48_dev, raw));
        }
        ads1015_data.fc_current = adc_map(ads1015_data.ads48[0], 1.494f, 0.484f, 0, 21);
        ads1015_data.fc_temp_c  = fccu_ntc_voltage_to_celsius(ads1015_data.ads48[1]);
        ads1015_data.hp_sensor  = ads1015_data.ads48[2];
        ads1015_data.lp_sensor  = ads1015_data.ads48[3];
    }

    if (s_ads49_ok) {
        for (int ch = 0; ch < 4; ch++) {
            int16_t raw                 = ads1015_read_channel_raw_single_shot(&ads1015_49_dev, ch);
            ads1015_data.ads49_raw[ch]  = raw;
            float v                     = mov_avg_push(&avg_ads49[ch],
                                                       ads1015_convert_raw_value_to_voltage(&ads1015_49_dev, raw));
            ads1015_data.ads49[ch]      = v;
            ads1015_data.ho_current[ch] = (v - ho_zero_v[ch]) * HO_I_PER_V;
        }
    }

    if (g_analog_log_enabled) {
        LOG_INF("FC_V: %.3f V  SC_V: %.3f V  FC_T: %.2f C  LP: %.4f bar  flow: %.3f Ln/min",
                (double)adc.fuel_cell_voltage.voltage, (double)adc.supercap_voltage.voltage,
                (double)adc.temp_c, (double)ads1015_data.lp_sensor, (double)flow_rate_lnmin);
    }
}

void fccu_ads1015_can_send()
{
    struct candef_fccu_currents_t currents_msg = {
        .ho_current_0 =
            candef_fccu_currents_ho_current_0_encode((double)ads1015_data.ho_current[0]),
        .ho_current_1 =
            candef_fccu_currents_ho_current_1_encode((double)ads1015_data.ho_current[1]),
        .ho_current_2 =
            candef_fccu_currents_ho_current_2_encode((double)ads1015_data.ho_current[2]),
        .ho_current_3 =
            candef_fccu_currents_ho_current_3_encode((double)ads1015_data.ho_current[3]),
    };
    uint8_t currents_buf[CANDEF_FCCU_CURRENTS_LENGTH];
    candef_fccu_currents_pack(currents_buf, &currents_msg, sizeof(currents_buf));
    can_send_(can.can_device, CANDEF_FCCU_CURRENTS_FRAME_ID, currents_buf, sizeof(currents_buf));
}

void fccu_bmp280_sensor_init()
{
    if (!sensor.sensor || !device_is_ready(sensor.sensor)) {
        LOG_ERR("BME280@76 not ready");
        return;
    }
    LOG_INF("BME280@76 ready: %s", sensor.sensor->name);
}

void fccu_bmp280_sensor2_init()
{
    if (!sensor2.sensor || !device_is_ready(sensor2.sensor)) {
        LOG_ERR("BME280@77 not ready");
        return;
    }
    LOG_INF("BME280@77 ready: %s", sensor2.sensor->name);
}

void fccu_bmp280_sensor_read()
{
    if (!sensor.sensor || !device_is_ready(sensor.sensor)) {
        return;
    }
    if (sensor_sample_fetch(sensor.sensor)) {
        LOG_WRN("BME280@76 fetch error — recovering I2C bus");
        i2c_recover_bus(DEVICE_DT_GET(DT_NODELABEL(i2c0)));
        return;
    }
    sensor_channel_get(sensor.sensor, SENSOR_CHAN_AMBIENT_TEMP, &sensor.temperature_buffer);
    sensor_channel_get(sensor.sensor, SENSOR_CHAN_PRESS, &sensor.pressure_buffer);
    sensor_channel_get(sensor.sensor, SENSOR_CHAN_HUMIDITY, &sensor.humidity_buffer);

    sensor.temperature =
        mov_avg_push(&avg_bme76_t, sensor_value_to_float(&sensor.temperature_buffer));
    sensor.pressure =
        mov_avg_push(&avg_bme76_p, sensor_value_to_float(&sensor.pressure_buffer) * 10.0f);
    sensor.humidity = mov_avg_push(&avg_bme76_h, sensor_value_to_float(&sensor.humidity_buffer));
}

void fccu_bmp280_sensor2_read()
{
    if (!sensor2.sensor || !device_is_ready(sensor2.sensor)) {
        return;
    }
    if (sensor_sample_fetch(sensor2.sensor)) {
        LOG_WRN("BME280@77 fetch error — recovering I2C bus");
        i2c_recover_bus(DEVICE_DT_GET(DT_NODELABEL(i2c0)));
        return;
    }
    sensor_channel_get(sensor2.sensor, SENSOR_CHAN_AMBIENT_TEMP, &sensor2.temperature_buffer);
    sensor_channel_get(sensor2.sensor, SENSOR_CHAN_PRESS, &sensor2.pressure_buffer);
    sensor_channel_get(sensor2.sensor, SENSOR_CHAN_HUMIDITY, &sensor2.humidity_buffer);

    sensor2.temperature =
        mov_avg_push(&avg_bme77_t, sensor_value_to_float(&sensor2.temperature_buffer));
    sensor2.pressure =
        mov_avg_push(&avg_bme77_p, sensor_value_to_float(&sensor2.pressure_buffer) * 10.0f);
    sensor2.humidity = mov_avg_push(&avg_bme77_h, sensor_value_to_float(&sensor2.humidity_buffer));
}

void fccu_environment_can_send()
{
    struct candef_fccu_environment_t environment_msg = {
        .fc_temp_c      = candef_fccu_environment_fc_temp_c_encode((double)ads1015_data.fc_temp_c),
        .ambient_temp_c = candef_fccu_environment_ambient_temp_c_encode((double)sensor.temperature),
        .ambient_humidity =
            candef_fccu_environment_ambient_humidity_encode((double)sensor.humidity),
        .ambient_pressure =
            candef_fccu_environment_ambient_pressure_encode((double)sensor.pressure),
    };
    uint8_t environment_buf[CANDEF_FCCU_ENVIRONMENT_LENGTH];
    candef_fccu_environment_pack(environment_buf, &environment_msg, sizeof(environment_buf));
    can_send_(can.can_device, CANDEF_FCCU_ENVIRONMENT_FRAME_ID, environment_buf,
              sizeof(environment_buf));
}
