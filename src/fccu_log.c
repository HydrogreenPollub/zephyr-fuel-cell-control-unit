#include "fccu_log.h"

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

static fccu_log_sample_t s_log[FCCU_LOG_CAPACITY];
static uint16_t s_head  = 0;
static uint16_t s_count = 0;

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
    s_log[s_head] = *s;
    s_head = (s_head + 1U) % FCCU_LOG_CAPACITY;
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
    *out_fc = s_log[idx].fc_v;
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
        shell_print(sh, "log empty");
        return 0;
    }

    shell_print(sh, "ts_ms,fc_v,sc_v,temp_fc_c,lp_bar,"
                    "ads48_0,ads48_1,ads48_2,ads48_3,"
                    "ads49_0,ads49_1,ads49_2,ads49_3,"
                    "bme76_t,bme76_h,bme76_p,"
                    "bme77_t,bme77_h,bme77_p");

    uint16_t base = oldest_idx();
    for (uint16_t n = 0; n < s_count; n++) {
        const fccu_log_sample_t *e = &s_log[(base + n) % FCCU_LOG_CAPACITY];
        shell_print(sh,
                    "%lld,%.3f,%.3f,%.3f,%.5f,"
                    "%.5f,%.5f,%.5f,%.5f,"
                    "%.5f,%.5f,%.5f,%.5f,"
                    "%.3f,%.3f,%.3f,"
                    "%.3f,%.3f,%.3f",
                    (long long)e->ts_ms,
                    (double)e->fc_v, (double)e->sc_v,
                    (double)e->temp_fc_c, (double)e->lp_bar,
                    (double)e->ads48[0], (double)e->ads48[1],
                    (double)e->ads48[2], (double)e->ads48[3],
                    (double)e->ads49[0], (double)e->ads49[1],
                    (double)e->ads49[2], (double)e->ads49[3],
                    (double)e->bme76_t, (double)e->bme76_h, (double)e->bme76_p,
                    (double)e->bme77_t, (double)e->bme77_h, (double)e->bme77_p);
    }
    return 0;
}

int cmd_flog_clear(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    fccu_log_clear();
    shell_print(sh, "log cleared");
    return 0;
}

int cmd_flog_status(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    shell_print(sh, "%u/%u samples", (unsigned)s_count, FCCU_LOG_CAPACITY);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_flog,
    SHELL_CMD(clear,  NULL, "Clear log buffer",     cmd_flog_clear),
    SHELL_CMD(status, NULL, "Show log fill level",  cmd_flog_status),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(logs, &sub_flog, "CSV data log (no arg = dump)", cmd_flog_dump);
