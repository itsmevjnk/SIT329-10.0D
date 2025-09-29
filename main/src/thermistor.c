#include "thermistor.h"
#include "vdiv.h"
#include "safe_adc.h"

#include <math.h>

#define T_KELVIN                    273.15 // 0C in Kelvin

/*
 * static float rt_calc(int voltage)
 *  Calculates the temperature given the thermistor's measurement.
 *  Inputs:
 *   - voltage : The measured voltage across the thermistor's pulldown resistor
 *               in mV.
 *  Output: The applied force in C.
 */
static float rt_calc(int voltage) {
    float R = calc_resistance(voltage, RT_R_PD); // calculate resistance
    float T = 1 / (log(R / RT_R0) / RT_B + 1 / (RT_t0 + T_KELVIN));
        // NOTE: see 5.2D task submission for explanation
    return T - T_KELVIN;
}

void rt_init() {
    adc_init_channel(RT_PIN_CHANNEL);
}

float rt_read(TickType_t max_wait) {
    int voltage;
    if (adc_read(RT_PIN_CHANNEL, &voltage, max_wait) != ESP_OK) return NAN;
    return rt_calc(voltage);
}