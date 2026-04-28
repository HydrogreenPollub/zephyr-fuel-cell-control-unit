#include "fccu_analog.h"

#include <math.h>
#include <zephyr/init.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

#include "ads1015.h"
#include "can.h"
#include "candef.h"

LOG_MODULE_REGISTER(fccu_analog, LOG_LEVEL_INF);


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
static bool s_ads48_ok = false;
static bool s_ads49_ok = false;

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
static mov_avg_t avg_fc_v, avg_sc_v;

static float mov_avg_push(mov_avg_t *m, float v)
{
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

void fccu_adc_init()
{
    adc_init(&adc.fuel_cell_voltage.adc_channel);
    adc_init(&adc.supercap_voltage.adc_channel);
    adc_init(&adc.temp_sensor.adc_channel);
}

void fccu_adc_read()
{
    adc_read_(&adc.fuel_cell_voltage.adc_channel, &adc.fuel_cell_voltage.raw_value);
    adc.fuel_cell_voltage.voltage = mov_avg_push(&avg_fc_v,
        adc_map((float)adc.fuel_cell_voltage.raw_value, 1.0f, 2853.8f, 0.4f, 52.0f));

    adc_read_(&adc.supercap_voltage.adc_channel, &adc.supercap_voltage.raw_value);
    adc.supercap_voltage.voltage = mov_avg_push(&avg_sc_v,
        adc_map((float)adc.supercap_voltage.raw_value, 1.0f, 2853.8f, 0.4f, 52.0f));

    adc_read_(&adc.temp_sensor.adc_channel, &adc.temp_sensor.raw_value);
    int32_t val = (int32_t)adc.temp_sensor.raw_value;
    adc_raw_to_millivolts_dt(&adc.temp_sensor.adc_channel, &val);
    adc.temp_sensor.voltage = (float)val * 1000.0f;

    float lp_v   = (float)val / 1000.0f;
    float lp_bar = (lp_v - 0.120f) / (2.805f - 0.120f);

    LOG_INF("FC_V: %.3f  SC_V: %.3f",
            (double)adc.fuel_cell_voltage.voltage,
            (double)adc.supercap_voltage.voltage);

    struct candef_fccu_power_t power_msg = {
        .fc_voltage = (uint16_t)(adc.fuel_cell_voltage.voltage * 1000.0f),
        .sc_voltage = (uint16_t)(adc.supercap_voltage.voltage  * 1000.0f),
        .fc_current = (uint16_t)(ads1015_data.fc_current        * 1000.0f),
        .fc_temp_c  = (int16_t) (ads1015_data.fc_temp_c         * 10.0f),
    };
    uint8_t power_buf[CANDEF_FCCU_POWER_LENGTH];
    candef_fccu_power_pack(power_buf, &power_msg, sizeof(power_buf));
    can_send_(can.can_device, CANDEF_FCCU_POWER_FRAME_ID, power_buf, sizeof(power_buf));

    struct candef_fccu_hydrogen_t hydrogen_msg = {
        .lp_pressure      = (uint16_t)(lp_bar                * 1000.0f),
        .hp_pressure      = (uint16_t)(ads1015_data.hp_sensor * 1000.0f),
        .leakage_voltage  = (uint16_t)(ads1015_data.ads48[3]  * 10000.0f),
    };
    uint8_t hydrogen_buf[CANDEF_FCCU_HYDROGEN_LENGTH];
    candef_fccu_hydrogen_pack(hydrogen_buf, &hydrogen_msg, sizeof(hydrogen_buf));
    can_send_(can.can_device, CANDEF_FCCU_HYDROGEN_FRAME_ID, hydrogen_buf, sizeof(hydrogen_buf));
}

static float read_ntc_temperature(float adc_voltage)
{
    const float VCC     = 1.88f;
    const float R_FIXED = 1000.0f;
    const float R0      = 1000.0f;
    const float BETA    = 3950.0f;
    const float T0      = 298.15f;

    float R_ntc = R_FIXED * adc_voltage / (VCC - adc_voltage);
    float invT  = (1.0f / T0) + (1.0f / BETA) * logf(R_ntc / R0);
    return (1.0f / invT) - 273.15f;
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
    LOG_INF("HO zero cal: %.4f %.4f %.4f %.4f",
            (double)ho_zero_v[0], (double)ho_zero_v[1],
            (double)ho_zero_v[2], (double)ho_zero_v[3]);
}

void fccu_ads1015_init()
{
    uint8_t probe = 0;

    i2c_recover_bus(ads48_i2c.bus);

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
            ads1015_data.ads48[ch] = mov_avg_push(&avg_ads48[ch],
                ads1015_read_channel_single_shot(&ads1015_48_dev, ch));
        }
        ads1015_data.fc_current = adc_map(ads1015_data.ads48[0], 1.494f, 0.484f, 0, 21);
        ads1015_data.fc_temp_c  = read_ntc_temperature(ads1015_data.ads48[1]);
        ads1015_data.hp_sensor  = ads1015_data.ads48[2];
        ads1015_data.lp_sensor  = ads1015_data.ads48[3];
    }

    if (s_ads49_ok) {
        for (int ch = 0; ch < 4; ch++) {
            float v = mov_avg_push(&avg_ads49[ch],
                          ads1015_read_channel_single_shot(&ads1015_49_dev, ch));
            ads1015_data.ads49[ch]      = v;
            ads1015_data.ho_current[ch] = (v - ho_zero_v[ch]) * HO_I_PER_V;
        }
    }

    LOG_INF("FC_I: %.4f A  T: %.2f C  HP: %.4f  LP: %.4f",
            (double)ads1015_data.fc_current, (double)ads1015_data.fc_temp_c,
            (double)ads1015_data.hp_sensor,  (double)ads1015_data.lp_sensor);

    struct candef_fccu_currents_t currents_msg = {
        .ho_current_0 = (int16_t)(ads1015_data.ho_current[0] * 1000.0f),
        .ho_current_1 = (int16_t)(ads1015_data.ho_current[1] * 1000.0f),
        .ho_current_2 = (int16_t)(ads1015_data.ho_current[2] * 1000.0f),
        .ho_current_3 = (int16_t)(ads1015_data.ho_current[3] * 1000.0f),
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
        LOG_ERR("BME280@76 fetch error");
        return;
    }
    sensor_channel_get(sensor.sensor, SENSOR_CHAN_AMBIENT_TEMP, &sensor.temperature_buffer);
    sensor_channel_get(sensor.sensor, SENSOR_CHAN_PRESS,        &sensor.pressure_buffer);
    sensor_channel_get(sensor.sensor, SENSOR_CHAN_HUMIDITY,     &sensor.humidity_buffer);

    sensor.temperature = mov_avg_push(&avg_bme76_t,
                             sensor_value_to_float(&sensor.temperature_buffer));
    sensor.pressure    = mov_avg_push(&avg_bme76_p,
                             sensor_value_to_float(&sensor.pressure_buffer) * 10.0f);
    sensor.humidity    = mov_avg_push(&avg_bme76_h,
                             sensor_value_to_float(&sensor.humidity_buffer));
}

void fccu_bmp280_sensor2_read()
{
    if (!sensor2.sensor || !device_is_ready(sensor2.sensor)) {
        return;
    }
    if (sensor_sample_fetch(sensor2.sensor)) {
        LOG_ERR("BME280@77 fetch error");
        return;
    }
    sensor_channel_get(sensor2.sensor, SENSOR_CHAN_AMBIENT_TEMP, &sensor2.temperature_buffer);
    sensor_channel_get(sensor2.sensor, SENSOR_CHAN_PRESS,        &sensor2.pressure_buffer);
    sensor_channel_get(sensor2.sensor, SENSOR_CHAN_HUMIDITY,     &sensor2.humidity_buffer);

    sensor2.temperature = mov_avg_push(&avg_bme77_t,
                              sensor_value_to_float(&sensor2.temperature_buffer));
    sensor2.pressure    = mov_avg_push(&avg_bme77_p,
                              sensor_value_to_float(&sensor2.pressure_buffer) * 10.0f);
    sensor2.humidity    = mov_avg_push(&avg_bme77_h,
                              sensor_value_to_float(&sensor2.humidity_buffer));

    struct candef_fccu_thermal_t thermal_msg = {
        .bme76_temp     = (int16_t) (sensor.temperature  * 10.0f),
        .bme76_humidity = (uint16_t)(sensor.humidity      * 10.0f),
        .bme77_temp     = (int16_t) (sensor2.temperature  * 10.0f),
        .bme77_humidity = (uint16_t)(sensor2.humidity     * 10.0f),
    };
    uint8_t thermal_buf[CANDEF_FCCU_THERMAL_LENGTH];
    candef_fccu_thermal_pack(thermal_buf, &thermal_msg, sizeof(thermal_buf));
    can_send_(can.can_device, CANDEF_FCCU_THERMAL_FRAME_ID, thermal_buf, sizeof(thermal_buf));
}
