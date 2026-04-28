#include "fccu_flow.h"
#include "can.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fccu_flow, LOG_LEVEL_INF);

static const struct gpio_dt_spec flowmeter_pin =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), flow_pulse_gpios);

static struct gpio_callback flow_cb_data;
static atomic_t             pulse_count;

float flow_rate_lnmin;
float flow_total_ln;

static void flow_pulse_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);
    atomic_inc(&pulse_count);
}

void fccu_flow_init()
{
    atomic_set(&pulse_count, 0);
    flow_rate_lnmin = 0.0f;
    flow_total_ln   = 0.0f;

    gpio_pin_configure_dt(&flowmeter_pin, GPIO_INPUT);
    gpio_init_callback(&flow_cb_data, flow_pulse_isr, BIT(flowmeter_pin.pin));
    gpio_add_callback(flowmeter_pin.port, &flow_cb_data);
    gpio_pin_interrupt_configure_dt(&flowmeter_pin, GPIO_INT_EDGE_TO_ACTIVE);
    LOG_INF("Flowmeter configured (GPIO %d)", flowmeter_pin.pin);
}

void fccu_flow_on_tick()
{
    uint32_t pulses  = (uint32_t)atomic_set(&pulse_count, 0);
    flow_rate_lnmin  = pulses * FLOW_LN_PER_PULSE * 60.0f;
    flow_total_ln   += pulses * FLOW_LN_PER_PULSE;
}

void fccu_flow_can_send()
{
    struct {
        uint32_t rate_mln_min;
        uint32_t total_mln;
    } payload = {
        .rate_mln_min = (uint32_t)(flow_rate_lnmin * 1000.0f),
        .total_mln    = (uint32_t)(flow_total_ln   * 1000.0f),
    };
    can_send_(can.can_device, 0x505u, (uint8_t *)&payload, sizeof(payload));
}
