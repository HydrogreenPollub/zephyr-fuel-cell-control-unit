#ifndef ADC_H
#define ADC_H

#ifdef __cplusplus
extern "C" {

#endif

#include <zephyr/drivers/adc.h>
#include "zephyr/logging/log.h"
#include <inttypes.h>

int adc_init(const struct adc_dt_spec *adc/*, struct adc_sequence *sequence, int16_t *buffer*/);

int adc_read_(const struct adc_dt_spec *adc, int16_t *buffer);

float adc_map(float x, float in_min, float in_max, float out_min, float out_max);

#ifdef __cplusplus
}
#endif

#endif //ADC_H
