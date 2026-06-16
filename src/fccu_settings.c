#include "fccu_settings.h"
#include "fccu_digital.h"
#include "fccu_fan.h"

#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(fccu_settings, LOG_LEVEL_INF);

#define LOAD(k, var)                                                                               \
    if (strcmp(key, (k)) == 0) {                                                                   \
        rc = read_cb(cb_arg, &(var), sizeof(var));                                                 \
    } else

int fccu_settings_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
    int rc = 0;

    LOAD("purge_mode", g_purge_mode)
    LOAD("purge_interval_s", g_purge_periodic_interval_s)
    LOAD("purge_duration_ms", g_purge_duration_ms)
    LOAD("purge_threshold_v", g_purge_threshold_v)
    LOAD("fc_v_source", g_fc_v_source)
    LOAD("fan_manual", g_fan_manual)
    LOAD("fan_target_c", g_fan_target_c)
    LOAD("fan_duty_pct", g_fan_manual_duty_pct)
    { /* unknown key — ignore */
    }

    return (rc < 0) ? rc : 0;
}

#undef LOAD

SETTINGS_STATIC_HANDLER_DEFINE(fccu, "fccu", NULL, fccu_settings_set, NULL, NULL);

void fccu_settings_init()
{
    int rc = settings_subsys_init();
    if (rc != 0) {
        LOG_ERR("Settings init failed: %d", rc);
        return;
    }
    settings_load();
    LOG_INF("Settings loaded from flash");
}

void fccu_settings_save()
{
    settings_save_one("fccu/purge_mode", &g_purge_mode, sizeof(g_purge_mode));
    settings_save_one("fccu/purge_interval_s", &g_purge_periodic_interval_s,
                      sizeof(g_purge_periodic_interval_s));
    settings_save_one("fccu/purge_duration_ms", &g_purge_duration_ms, sizeof(g_purge_duration_ms));
    settings_save_one("fccu/purge_threshold_v", &g_purge_threshold_v, sizeof(g_purge_threshold_v));
    settings_save_one("fccu/fc_v_source", &g_fc_v_source, sizeof(g_fc_v_source));
    settings_save_one("fccu/fan_manual", &g_fan_manual, sizeof(g_fan_manual));
    settings_save_one("fccu/fan_target_c", &g_fan_target_c, sizeof(g_fan_target_c));
    settings_save_one("fccu/fan_duty_pct", &g_fan_manual_duty_pct, sizeof(g_fan_manual_duty_pct));
    LOG_INF("Settings saved to flash");
}
