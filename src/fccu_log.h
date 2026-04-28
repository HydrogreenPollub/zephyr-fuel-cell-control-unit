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

/* Append one sample to the circular buffer, evicting the oldest if full. */
void fccu_log_add(const fccu_log_sample_t *s);

/* Read the fc_v value from samples_ago steps before the newest entry.
 * Returns false if the buffer does not contain enough samples. */
bool fccu_log_get_fc_ago(uint16_t samples_ago, float *out_fc);

/* Reset the circular buffer. */
void fccu_log_clear();

/* Return the number of samples currently stored. */
size_t fccu_log_count();

#endif
