#include "fccu_log.h"
#include "fccu_analog.h"

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

static fccu_log_sample_t s_log[FCCU_LOG_CAPACITY];
static uint16_t          s_head  = 0;
static uint16_t          s_count = 0;

bool g_log_enabled = true;

static uint16_t oldest_idx()
{
    return (s_head + FCCU_LOG_CAPACITY - s_count) % FCCU_LOG_CAPACITY;
}

static uint16_t newest_idx()
{
    return (s_head + FCCU_LOG_CAPACITY - 1U) % FCCU_LOG_CAPACITY;
}

void fccu_log_add(const fccu_log_sample_t *s)
{
    if (!g_log_enabled) {
        return;
    }
    s_log[s_head] = *s;
    s_head        = (s_head + 1U) % FCCU_LOG_CAPACITY;
    if (s_count < FCCU_LOG_CAPACITY) {
        s_count++;
    }
}

bool fccu_log_get_fc_ago(uint16_t samples_ago, float *out_fc)
{
    if (!out_fc || s_count == 0 || samples_ago >= s_count) {
        return false;
    }
    uint16_t idx = (newest_idx() + FCCU_LOG_CAPACITY - samples_ago) % FCCU_LOG_CAPACITY;
    *out_fc      = s_log[idx].fc_v;
    return true;
}

void fccu_log_clear()
{
    s_head  = 0;
    s_count = 0;
}

size_t fccu_log_count()
{
    return s_count;
}

int cmd_flog_dump(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (s_count == 0) {
        shell_print(sh, "Log empty");
        return 0;
    }

    shell_print(sh, "ts_ms,fc_v,sc_v,lp_bar,flow_lnmin,fan_pct,ntc_t,"
                    "ho0,ho1,ho2,ho3,"
                    "bme76_t,bme76_h,bme76_p,"
                    "bme77_t,bme77_h,bme77_p");

    uint16_t base = oldest_idx();
    for (uint16_t n = 0; n < s_count; n++) {
        const fccu_log_sample_t *e = &s_log[(base + n) % FCCU_LOG_CAPACITY];
        shell_print(sh,
                    "%lld,%.3f,%.3f,%.5f,%.3f,%u,%.2f,"
                    "%.4f,%.4f,%.4f,%.4f,"
                    "%.2f,%.1f,%.2f,"
                    "%.2f,%.1f,%.2f",
                    (long long)e->ts_ms, (double)e->fc_v, (double)e->sc_v, (double)e->lp_bar,
                    (double)e->flow_rate, e->fan_duty, (double)e->ntc_t, (double)e->ho_current[0],
                    (double)e->ho_current[1], (double)e->ho_current[2], (double)e->ho_current[3],
                    (double)e->bme76_t, (double)e->bme76_h, (double)e->bme76_p, (double)e->bme77_t,
                    (double)e->bme77_h, (double)e->bme77_p);
    }
    return 0;
}

int cmd_flog_clear(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    fccu_log_clear();
    shell_print(sh, "Log cleared");
    return 0;
}

int cmd_flog_status(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    shell_print(sh, "%u/%u samples  logging=%s  console=%s", (unsigned)s_count, FCCU_LOG_CAPACITY,
                g_log_enabled ? "on" : "off", g_analog_log_enabled ? "on" : "off");
    return 0;
}

int cmd_flog_enable(const struct shell *sh, size_t argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "console") == 0) {
        g_analog_log_enabled = true;
        shell_print(sh, "Console logging enabled");
    } else {
        g_log_enabled = true;
        shell_print(sh, "Logging enabled");
    }
    return 0;
}

int cmd_flog_disable(const struct shell *sh, size_t argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "console") == 0) {
        g_analog_log_enabled = false;
        shell_print(sh, "Console logging disabled");
    } else {
        g_log_enabled = false;
        shell_print(sh, "Logging disabled");
    }
    return 0;
}

static int cmd_logs_default(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    shell_print(sh, "%u/%u samples  logging=%s  console=%s", (unsigned)s_count, FCCU_LOG_CAPACITY,
                g_log_enabled ? "on" : "off", g_analog_log_enabled ? "on" : "off");
    shell_print(sh, "  dump              Dump all samples as CSV");
    shell_print(sh, "  clear             Clear log buffer");
    shell_print(sh, "  status            Show fill level and logging state");
    shell_print(sh, "  enable [console]   Enable cyclic / console logging");
    shell_print(sh, "  disable [console]  Disable cyclic / console logging");
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_flog, SHELL_CMD(dump, NULL, "Dump all samples as CSV", cmd_flog_dump),
    SHELL_CMD(clear, NULL, "Clear log buffer", cmd_flog_clear),
    SHELL_CMD(status, NULL, "Show fill level and state", cmd_flog_status),
    SHELL_CMD_ARG(enable, NULL, "Enable cyclic or console logging", cmd_flog_enable, 1, 1),
    SHELL_CMD_ARG(disable, NULL, "Disable cyclic or console logging", cmd_flog_disable, 1, 1),
    SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(logs, &sub_flog, "Log status and control", cmd_logs_default);
