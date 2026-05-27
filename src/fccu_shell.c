#include "fccu_shell.h"
#include "fccu.h"
#include "fccu_analog.h"
#include "fccu_can_rx.h"
#include "fccu_digital.h"
#include "fccu_fan.h"
#include "fccu_flow.h"
#include "fccu_settings.h"
#include "can.h"

#include <stdlib.h>
#include <string.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/reboot.h>

/* â”€â”€ status â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

static void print_status(const struct shell *sh)
{
    shell_print(sh, "%-16s %s", "machine_state:",
                state == RUNNING   ? "RUNNING"
                : state == STOPPED ? "STOPPED"
                                    : "IDLE");

    shell_print(sh, "%-16s %.3f V  (raw: %d)", "fc_v:", (double)adc.fuel_cell_voltage.voltage,
                adc.fuel_cell_voltage.raw_value);
    shell_print(sh, "%-16s %.4f A  (v: %.4f V, raw: %d)", "fc_i:",
                (double)ads1015_data.fc_current, (double)ads1015_data.ads48[0],
                ads1015_data.ads48_raw[0]);
    shell_print(sh, "%-16s %.3f V  (raw: %d)", "power_supply_v:",
                (double)adc.supercap_voltage.voltage,
                adc.supercap_voltage.raw_value);
    shell_print(sh, "%-16s %.2f C  (raw: %d, v: %.4f V)", "fc_t:", (double)adc.temp_c,
                adc.temp_sensor.raw_value, (double)adc.temp_sensor.voltage);
    shell_print(sh, "%-16s %.2f C  (hp: %.4f V, lp: %.4f V)", "fc_ads:",
                (double)ads1015_data.fc_temp_c, (double)ads1015_data.hp_sensor,
                (double)ads1015_data.lp_sensor);
    shell_print(sh, "%-16s %.3f Ln/min  (total: %.3f Ln)", "flow:", (double)flow_rate_lnmin,
                (double)flow_total_ln);

    shell_print(sh, "");

    shell_print(sh, "%-16s %.4f  %.4f  %.4f  %.4f V", "ads48:", (double)ads1015_data.ads48[0],
                (double)ads1015_data.ads48[1], (double)ads1015_data.ads48[2],
                (double)ads1015_data.ads48[3]);
    shell_print(sh, "%-16s %.4f  %.4f  %.4f  %.4f A", "ho:", (double)ads1015_data.ho_current[0],
                (double)ads1015_data.ho_current[1], (double)ads1015_data.ho_current[2],
                (double)ads1015_data.ho_current[3]);
    shell_print(sh, "%-16s %.4f  %.4f  %.4f  %.4f", "ho_zero:", (double)ho_zero_v[0], (double)ho_zero_v[1],
                (double)ho_zero_v[2], (double)ho_zero_v[3]);

    shell_print(sh, "");

    shell_print(sh, "%-16s T=%.2f C  h=%.1f%%  p=%.2f hPa", "bme77_internal:",
                (double)sensor2.temperature,
                (double)sensor2.humidity, (double)sensor2.pressure);
    shell_print(sh, "%-16s T=%.2f C  h=%.1f%%  p=%.2f hPa", "bme76_external:",
                (double)sensor.temperature,
                (double)sensor.humidity, (double)sensor.pressure);

    shell_print(sh, "");

    if (g_fan_manual) {
        shell_print(sh, "%-16s duty=%u%%  mode=manual", "fan:", fan_pwm_percent);
    } else {
        shell_print(sh, "%-16s duty=%u%%  mode=auto  target=%.1f C", "fan:", fan_pwm_percent,
                    (double)g_fan_target_c);
    }

    shell_print(sh, "%-16s state=%s", "main_valve:", flags.main_valve_on ? "open" : "closed");

    const char *mode_str = g_purge_mode == PURGE_MODE_THRESHOLD  ? "threshold"
                           : g_purge_mode == PURGE_MODE_PERIODIC ? "periodic"
                                                                 : "manual";
    shell_print(sh,
                "%-16s state=%s  mode=%s  interval=%u s  threshold=%.2f V  duration=%u ms",
                "purge_valve:",
                flags.purge_valve_on ? "open" : "closed", mode_str, g_purge_periodic_interval_s,
                (double)g_purge_threshold_v, g_purge_duration_ms);
}

int cmd_status(const struct shell *sh, size_t argc, char **argv)
{
    if (argc == 1) {
        print_status(sh);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        shell_print(sh, "usage: status [--periodic <s>]");
        shell_print(sh, "Print current FCCU sensor values, actuator state, and purge/fan settings.");
        shell_print(sh, "  --periodic <s>  Repeat every <s> seconds until interrupted");
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "--periodic") == 0) {
        int period_s = atoi(argv[2]);
        if (period_s <= 0) {
            shell_print(sh, "usage: status --periodic <s>");
            return -EINVAL;
        }

        while (1) {
            print_status(sh);
            shell_print(sh, "");
            k_sleep(K_SECONDS(period_s));
        }
    }

    shell_print(sh, "usage: status [--periodic <s>]");
    return 0;
}

SHELL_CMD_ARG_REGISTER(status, NULL, "Show status; use --periodic <s> to repeat", cmd_status, 1, 2);

/* â”€â”€ adc â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

int cmd_adc(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    shell_print(sh, "fc_v_raw:%d  -> %.3f V", (int)adc.fuel_cell_voltage.raw_value,
                (double)adc.fuel_cell_voltage.voltage);
    shell_print(sh, "sc_v_raw:%d  -> %.3f V", (int)adc.supercap_voltage.raw_value,
                (double)adc.supercap_voltage.voltage);
    shell_print(sh, "ntc_raw: %d  -> %.4f V  -> %.2f C", (int)adc.temp_sensor.raw_value,
                (double)adc.temp_sensor.voltage, (double)adc.temp_c);
    shell_print(sh, "ads48:   %.5f %.5f %.5f %.5f V", (double)ads1015_data.ads48[0],
                (double)ads1015_data.ads48[1], (double)ads1015_data.ads48[2],
                (double)ads1015_data.ads48[3]);
    shell_print(sh, "ads49:   %.5f %.5f %.5f %.5f A", (double)ads1015_data.ho_current[0],
                (double)ads1015_data.ho_current[1], (double)ads1015_data.ho_current[2],
                (double)ads1015_data.ho_current[3]);
    return 0;
}

SHELL_CMD_REGISTER(adc, NULL, "Show raw ADC values", cmd_adc);

/* â”€â”€ fan â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

int cmd_fan_target(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(sh, "usage: fan target <Â°C>");
        return -EINVAL;
    }
    g_fan_target_c = strtof(argv[1], NULL);
    g_fan_manual   = false;
    shell_print(sh, "Fan target: %.1f C", (double)g_fan_target_c);
    return 0;
}

int cmd_fan_duty(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(sh, "usage: fan duty <0-100>");
        return -EINVAL;
    }
    int d = atoi(argv[1]);
    if (d < 0 || d > 100) {
        shell_print(sh, "Duty must be 0-100");
        return -EINVAL;
    }
    g_fan_manual          = true;
    g_fan_manual_duty_pct = (uint8_t)d;
    fan_pwm_percent       = g_fan_manual_duty_pct;
    fccu_fan_pwm_set(fan_pwm_percent);
    shell_print(sh, "Fan manual duty: %d%%", d);
    return 0;
}

int cmd_fan_auto(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    g_fan_manual = false;
    shell_print(sh, "fan auto  target=%.1f C", (double)g_fan_target_c);
    return 0;
}

int cmd_fan(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    if (g_fan_manual) {
        shell_print(sh, "fan:  duty=%u%%  mode=manual", fan_pwm_percent);
    } else {
        shell_print(sh, "fan:  duty=%u%%  mode=auto  target=%.1f C", fan_pwm_percent,
                    (double)g_fan_target_c);
    }
    shell_print(sh, "  target <Â°C>   Set auto target temperature");
    shell_print(sh, "  duty <0-100>  Set manual duty cycle");
    shell_print(sh, "  auto          Enable auto control");
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_fan, SHELL_CMD_ARG(target, NULL, "Set fan target temperature [Â°C]", cmd_fan_target, 2, 0),
    SHELL_CMD_ARG(duty, NULL, "Set fan manual duty [0-100]", cmd_fan_duty, 2, 0),
    SHELL_CMD(auto, NULL, "Enable auto fan control", cmd_fan_auto), SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(fan, &sub_fan, "Fan status and control", cmd_fan);

/* â”€â”€ valve â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

int cmd_valve_on(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    fccu_main_valve_on();
    shell_print(sh, "Main valve open");
    return 0;
}

int cmd_valve_off(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    fccu_main_valve_off();
    shell_print(sh, "Main valve closed");
    return 0;
}

int cmd_valve(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    shell_print(sh, "main_valve: state=%s", flags.main_valve_on ? "open" : "closed");
    shell_print(sh, "  on   Open main hydrogen valve");
    shell_print(sh, "  off  Close main hydrogen valve");
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_valve, SHELL_CMD(on, NULL, "Open main valve", cmd_valve_on),
                               SHELL_CMD(off, NULL, "Close main valve", cmd_valve_off),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(valve, &sub_valve, "Main valve control", cmd_valve);

/* â”€â”€ purge â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

int cmd_purge_trigger(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    fccu_purge_valve_on();
    shell_print(sh, "Purge triggered (%u ms)", g_purge_duration_ms);
    return 0;
}

int cmd_purge_mode(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(sh, "usage: purge mode <threshold|periodic|manual>");
        return -EINVAL;
    }
    if (strcmp(argv[1], "threshold") == 0) {
        g_purge_mode = PURGE_MODE_THRESHOLD;
    } else if (strcmp(argv[1], "periodic") == 0) {
        g_purge_mode = PURGE_MODE_PERIODIC;
    } else if (strcmp(argv[1], "manual") == 0) {
        g_purge_mode = PURGE_MODE_MANUAL;
    } else {
        shell_print(sh, "usage: purge mode <threshold|periodic|manual>");
        return -EINVAL;
    }
    shell_print(sh, "Purge mode: %s", argv[1]);
    return 0;
}

int cmd_purge_threshold(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(sh, "usage: purge threshold <V>");
        return -EINVAL;
    }
    g_purge_threshold_v = strtof(argv[1], NULL);
    shell_print(sh, "Purge threshold: %.2f V drop", (double)g_purge_threshold_v);
    return 0;
}

int cmd_purge_interval(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(sh, "usage: purge interval <s>");
        return -EINVAL;
    }
    int s = atoi(argv[1]);
    if (s <= 0) {
        shell_print(sh, "Interval must be > 0");
        return -EINVAL;
    }
    g_purge_periodic_interval_s = (uint32_t)s;
    shell_print(sh, "Purge interval: %d s", s);
    return 0;
}

int cmd_purge_duration(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(sh, "usage: purge duration <ms>");
        return -EINVAL;
    }
    int ms = atoi(argv[1]);
    if (ms <= 0) {
        shell_print(sh, "Duration must be > 0");
        return -EINVAL;
    }
    g_purge_duration_ms = (uint32_t)ms;
    shell_print(sh, "Purge duration: %d ms", ms);
    return 0;
}

int cmd_purge(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    const char *mode_str = g_purge_mode == PURGE_MODE_THRESHOLD  ? "threshold"
                           : g_purge_mode == PURGE_MODE_PERIODIC ? "periodic"
                                                                 : "manual";
    shell_print(sh,
                "purge_valve: state=%s  mode=%s  interval=%u s  threshold=%.2f V  duration=%u ms",
                flags.purge_valve_on ? "open" : "closed", mode_str, g_purge_periodic_interval_s,
                (double)g_purge_threshold_v, g_purge_duration_ms);
    shell_print(sh, "  trigger                          Trigger purge immediately");
    shell_print(sh, "  mode <threshold|periodic|manual> Set purge trigger mode");
    shell_print(sh, "  threshold <V>                    Set FC drop threshold (V)");
    shell_print(sh, "  interval <s>                     Set periodic interval (s)");
    shell_print(sh, "  duration <ms>                    Set pulse duration (ms)");
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_purge, SHELL_CMD(trigger, NULL, "Trigger purge immediately", cmd_purge_trigger),
    SHELL_CMD_ARG(mode, NULL, "Set purge mode [threshold|periodic|manual]", cmd_purge_mode, 2, 0),
    SHELL_CMD_ARG(threshold, NULL, "Set FC drop threshold [V]", cmd_purge_threshold, 2, 0),
    SHELL_CMD_ARG(interval, NULL, "Set periodic interval [s]", cmd_purge_interval, 2, 0),
    SHELL_CMD_ARG(duration, NULL, "Set pulse duration [ms]", cmd_purge_duration, 2, 0),
    SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(purge, &sub_purge, "Purge valve status and control", cmd_purge);

/* â”€â”€ flow â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

int cmd_flow(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    shell_print(sh, "flow_rate:\t%.3f Ln/min", (double)flow_rate_lnmin);
    shell_print(sh, "flow_total:\t%.3f Ln", (double)flow_total_ln);
    return 0;
}

SHELL_CMD_REGISTER(flow, NULL, "Show flow rate and accumulated total", cmd_flow);

/* â”€â”€ send â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

int cmd_send(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 3) {
        shell_print(sh, "usage: send <id_hex> <float>");
        return -EINVAL;
    }
    uint16_t id  = (uint16_t)strtoul(argv[1], NULL, 16);
    float    v   = strtof(argv[2], NULL);
    int      ret = can_send_float(can.can_device, id, v);
    if (ret == 0) {
        shell_print(sh, "Sent 0x%03x = %.4f", id, (double)v);
    } else {
        shell_print(sh, "CAN send failed: %d", ret);
    }
    return 0;
}

SHELL_CMD_REGISTER(send, NULL, "Send CAN float: send <id_hex> <float>", cmd_send);

/* â”€â”€ can â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

int cmd_can(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (mcu_data.last_rx_ms == 0) {
        shell_print(sh, "No MCU frames received yet");
        return 0;
    }

    int64_t age_ms = k_uptime_get() - mcu_data.last_rx_ms;
    shell_print(sh, "MCU  (last rx: %lld ms ago)", (long long)age_ms);
    shell_print(sh, "  fc_v:      %.3f V    fc_c:  %.4f A", (double)mcu_data.fc_v,
                (double)mcu_data.fc_c);
    shell_print(sh, "  sc_v:      %.3f V    sc_c:  %.4f A", (double)mcu_data.sc_v,
                (double)mcu_data.sc_c);
    shell_print(sh, "  mc_v:      %.3f V    mc_c:  %.4f A", (double)mcu_data.mc_v,
                (double)mcu_data.mc_c);
    shell_print(sh, "  leakage_v: %.4f V", (double)mcu_data.leakage_v);
    return 0;
}

SHELL_CMD_REGISTER(can, NULL, "Show latest MCU CAN telemetry", cmd_can);

/* â”€â”€ reboot â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

int cmd_reboot(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    fccu_settings_save();
    shell_print(sh, "Settings saved. Rebooting...");
    k_msleep(500);
    sys_reboot(SYS_REBOOT_COLD);
    return 0;
}

SHELL_CMD_REGISTER(reboot, NULL, "Reboot the device", cmd_reboot);

/* â”€â”€ settings â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

int cmd_settings_save(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    fccu_settings_save();
    shell_print(sh, "settings saved to flash");
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_settings,
                               SHELL_CMD(save, NULL, "Save settings to flash", cmd_settings_save),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(settings, &sub_settings, "Persist settings to flash", NULL);
