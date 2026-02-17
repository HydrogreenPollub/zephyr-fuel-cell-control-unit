#include "adc.h"

LOG_MODULE_REGISTER(adc);

int adc_init(const struct adc_dt_spec *adc/*, struct adc_sequence *sequence, int16_t *buffer*/) {
    if (!device_is_ready(adc->dev)) {
        LOG_ERR("ADC device not ready\n");
        return -1;
    }

    if (adc_channel_setup_dt(adc) != 0) {
        LOG_ERR("ADC channel setup failed\n");
        return -1;
    }

    return 0;
}

int adc_read_(const struct adc_dt_spec *adc, int16_t *buffer) {

    struct adc_sequence sequence = {
        .channels = BIT(adc->channel_id),
        .buffer = buffer,
        .buffer_size = sizeof(*buffer),
        .resolution = adc->resolution,
    };

    int ret = adc_read_dt(adc, &sequence);

    if (ret != 0) {
        LOG_ERR("ADC read failed (err %d)", ret);
        return ret;
    }
    return 0;
}

float adc_map(float x, float in_min, float in_max, float out_min, float out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
