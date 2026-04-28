#ifndef FCCU_LOG_H
#define FCCU_LOG_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/** @brief Maximum number of samples retained in the circular log buffer. */
#define FCCU_LOG_CAPACITY 200

/**
 * @brief One measurement snapshot stored in the log buffer.
 *
 * All fields are populated by fccu_on_tick() once per second.
 */
typedef struct {
    int64_t ts_ms;     /**< Kernel uptime timestamp in milliseconds. */
    float   fc_v;      /**< Fuel cell stack voltage (V). */
    float   sc_v;      /**< Supercapacitor voltage (V). */
    float   temp_fc_c; /**< Fuel cell NTC temperature (°C). */
    float   lp_bar;    /**< Low-pressure hydrogen sensor reading (bar). */
    float   ads48[4];  /**< ADS1015@48 channel voltages (V). */
    float   ads49[4];  /**< ADS1015@49 HO-10P currents (A). */
    float   bme76_t;   /**< BME280@76 temperature (°C). */
    float   bme76_h;   /**< BME280@76 relative humidity (%). */
    float   bme76_p;   /**< BME280@76 pressure (hPa × 10). */
    float   bme77_t;   /**< BME280@77 temperature (°C). */
    float   bme77_h;   /**< BME280@77 relative humidity (%). */
    float   bme77_p;   /**< BME280@77 pressure (hPa × 10). */
} fccu_log_sample_t;

/**
 * @brief Append one sample to the circular log buffer.
 *
 * If the buffer is full the oldest sample is silently overwritten.
 *
 * @param s Pointer to the sample to copy into the buffer.
 */
void fccu_log_add(const fccu_log_sample_t *s);

/**
 * @brief Read the fuel cell voltage from a historical log entry.
 *
 * Counts back @p samples_ago steps from the newest entry and writes the
 * stored fc_v value to @p out_fc.
 *
 * @param samples_ago Number of steps before the newest entry (0 = newest).
 * @param out_fc      Output pointer for the retrieved voltage value.
 * @return true if the entry exists; false if the buffer has fewer than
 *         (samples_ago + 1) entries or @p out_fc is NULL.
 */
bool fccu_log_get_fc_ago(uint16_t samples_ago, float *out_fc);

/**
 * @brief Reset the circular log buffer.
 *
 * Discards all stored samples by resetting the head index and count to zero.
 */
void fccu_log_clear();

/**
 * @brief Return the number of samples currently stored in the buffer.
 *
 * @return Sample count in the range [0, FCCU_LOG_CAPACITY].
 */
size_t fccu_log_count();

#endif
