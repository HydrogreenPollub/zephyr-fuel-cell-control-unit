#ifndef FCCU_FLOW_H
#define FCCU_FLOW_H

#include "fccu.h"

extern float
    flow_rate_lnmin; /**< Current volumetric flow rate in normalised litres per minute (Ln/min). */
extern float flow_total_ln; /**< Accumulated total flow volume in normalised litres (Ln). */

/**
 * @brief Configure the flowmeter pulse input GPIO and attach the edge ISR.
 *
 * Sets up the flow-pulse-gpios pin defined in the devicetree zephyr,user node
 * as an input with a falling-edge interrupt. The ISR increments an atomic
 * pulse counter on each detected pulse from the Vogtlin flowmeter.
 */
void fccu_flow_init();

/**
 * @brief Compute flow rate and accumulate total volume from the pulse counter.
 *
 * Atomically reads and resets the pulse counter since the last call, then
 * updates flow_rate_lnmin and flow_total_ln using FLOW_LN_PER_PULSE.
 * Should be called exactly once per second from the 1 Hz measurement tick.
 */
void fccu_flow_on_tick();

#endif /* FCCU_FLOW_H */
