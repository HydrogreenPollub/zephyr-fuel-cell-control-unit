#include "fccu.h"
#include "fccu_analog.h"
#include "fccu_fan.h"
#include "fccu_digital.h"
#include "can.h"

#include <stdlib.h>
#include <zephyr/shell/shell.h>


static int cmd_status(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    shell_print(sh, "state: %s",
                state == RUNNING ? "RUNNING" : state == STOPPED ? "STOPPED" : "IDLE");
    shell_print(sh, "fc_v:    %.3f V", (double)adc.fuel_cell_voltage.voltage);
    shell_print(sh, "sc_v:    %.3f V", (double)adc.supercap_voltage.voltage);
    shell_print(sh, "fc_i:    %.4f A", (double)ads1015_data.fc_current);
    shell_print(sh, "fc_t:    %.2f C", (double)ads1015_data.fc_temp_c);
    shell_print(sh, "hp:      %.4f V", (double)ads1015_data.hp_sensor);
    shell_print(sh, "lp:      %.4f V", (double)ads1015_data.lp_sensor);
    shell_print(sh, "bme76:   t=%.2f h=%.1f p=%.2f",
                (double)sensor.temperature,
                (double)sensor.humidity,
                (double)sensor.pressure);
    shell_print(sh, "bme77:   t=%.2f h=%.1f p=%.2f",
                (double)sensor2.temperature,
                (double)sensor2.humidity,
                (double)sensor2.pressure);
    shell_print(sh, "fan:     %u%%  (target %.1f C, manual=%d)",
                fan_pwm_percent,
                (double)g_fan_target_c, (int)g_fan_manual);
    shell_print(sh, "valve:   main=%d purge=%d setpoint=%u%%",
                (int)flags.main_valve_on, (int)flags.purge_valve_on,
                g_main_valve_setpoint_pct);
    shell_print(sh, "ho_zero: %.4f %.4f %.4f %.4f",
                (double)ho_zero_v[0], (double)ho_zero_v[1],
                (double)ho_zero_v[2], (double)ho_zero_v[3]);
    return 0;
}

SHELL_CMD_REGISTER(status, NULL, "Show all sensor values and state", cmd_status);


static int cmd_adc(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    shell_print(sh, "lp_raw:  %d", (int)adc.low_pressure_sensor.raw_value);
    shell_print(sh, "fc_v_raw:%d  -> %.3f V",
                (int)adc.fuel_cell_voltage.raw_value,
                (double)adc.fuel_cell_voltage.voltage);
    shell_print(sh, "sc_v_raw:%d  -> %.3f V",
                (int)adc.supercap_voltage.raw_value,
                (double)adc.supercap_voltage.voltage);
    shell_print(sh, "ads48:   %.5f %.5f %.5f %.5f V",
                (double)ads1015_data.ads48[0], (double)ads1015_data.ads48[1],
                (double)ads1015_data.ads48[2], (double)ads1015_data.ads48[3]);
    shell_print(sh, "ads49:   %.5f %.5f %.5f %.5f A",
                (double)ads1015_data.ho_current[0], (double)ads1015_data.ho_current[1],
                (double)ads1015_data.ho_current[2], (double)ads1015_data.ho_current[3]);
    return 0;
}

SHELL_CMD_REGISTER(adc, NULL, "Show raw ADC values", cmd_adc);


static int cmd_purge(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    fccu_purge_valve_on();
    shell_print(sh, "purge triggered");
    return 0;
}

SHELL_CMD_REGISTER(purge, NULL, "Trigger purge valve", cmd_purge);


static int cmd_fan_target(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(sh, "usage: fan target <°C>");
        return -EINVAL;
    }
    g_fan_target_c = strtof(argv[1], NULL);
    g_fan_manual   = false;
    shell_print(sh, "fan target: %.1f C", (double)g_fan_target_c);
    return 0;
}

static int cmd_fan_duty(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(sh, "usage: fan duty <0-100>");
        return -EINVAL;
    }
    int d = atoi(argv[1]);
    if (d < 0 || d > 100) {
        shell_print(sh, "duty must be 0-100");
        return -EINVAL;
    }
    g_fan_manual         = true;
    g_fan_manual_duty_pct = (uint8_t)d;
    shell_print(sh, "fan manual duty: %d%%", d);
    return 0;
}

static int cmd_fan_auto(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    g_fan_manual = false;
    shell_print(sh, "fan auto control enabled (target %.1f C)", (double)g_fan_target_c);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_fan,
    SHELL_CMD_ARG(target, NULL, "Set fan target temperature [°C]", cmd_fan_target, 2, 0),
    SHELL_CMD_ARG(duty,   NULL, "Set fan manual duty [0-100]",     cmd_fan_duty,   2, 0),
    SHELL_CMD(auto,   NULL, "Enable auto fan control",         cmd_fan_auto),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(fan, &sub_fan, "Fan control (target, duty, auto)", NULL);


static int cmd_valve_main(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(sh, "usage: valve main <0-100>");
        return -EINVAL;
    }
    int pct = atoi(argv[1]);
    if (pct < 0 || pct > 100) {
        shell_print(sh, "setpoint must be 0-100");
        return -EINVAL;
    }
    g_main_valve_setpoint_pct = (uint8_t)pct;
    shell_print(sh, "main valve setpoint: %d%%", pct);
    return 0;
}

static int cmd_valve_purge(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(sh, "usage: valve purge <ms>");
        return -EINVAL;
    }
    int ms = atoi(argv[1]);
    if (ms <= 0) {
        shell_print(sh, "duration must be > 0");
        return -EINVAL;
    }
    gpio_set(&valve_pin.purge_valve_on_pin);
    flags.purge_valve_on = true;
    k_msleep(ms);
    gpio_reset(&valve_pin.purge_valve_on_pin);
    flags.purge_valve_on = false;
    shell_print(sh, "purge pulse: %d ms", ms);
    return 0;
}

static int cmd_valve_trigger(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(sh, "usage: valve trigger <V>");
        return -EINVAL;
    }
    g_purge_trigger_v = strtof(argv[1], NULL);
    shell_print(sh, "purge trigger: %.2f V drop", (double)g_purge_trigger_v);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_valve,
    SHELL_CMD_ARG(main,    NULL, "Set main valve setpoint [0-100%%]",   cmd_valve_main,    2, 0),
    SHELL_CMD_ARG(purge,   NULL, "Pulse purge valve [ms]",              cmd_valve_purge,   2, 0),
    SHELL_CMD_ARG(trigger, NULL, "Set auto-purge FC drop threshold [V]",cmd_valve_trigger, 2, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(valve, &sub_valve, "Valve control (main, purge, trigger)", NULL);

static int cmd_send(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 3) {
        shell_print(sh, "usage: send <id_hex> <float>");
        return -EINVAL;
    }
    uint16_t id = (uint16_t)strtoul(argv[1], NULL, 16);
    float    v  = strtof(argv[2], NULL);
    int ret = can_send_float(can.can_device, id, v);
    if (ret == 0) {
        shell_print(sh, "sent 0x%03x = %.4f", id, (double)v);
    } else {
        shell_print(sh, "CAN send failed: %d", ret);
    }
    return 0;
}

SHELL_CMD_REGISTER(send, NULL, "Send CAN float: send <id_hex> <float>", cmd_send);
