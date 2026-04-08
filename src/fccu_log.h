#ifndef FCCU_LOG_H
#define FCCU_LOG_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define FCCU_LOG_CAPACITY 200

typedef struct {
    int64_t ts_ms;
    float   fc_v;
    float   sc_v;
    float   temp_fc_c;
    float   lp_bar;
    float   ads48[4];
    float   ads49[4];
    float   bme76_t, bme76_h, bme76_p;
    float   bme77_t, bme77_h, bme77_p;
} fccu_log_sample_t;

void   fccu_log_add(const fccu_log_sample_t *s);
bool   fccu_log_get_fc_ago(uint16_t samples_ago, float *out_fc);
void   fccu_log_clear();
size_t fccu_log_count();

#endif
