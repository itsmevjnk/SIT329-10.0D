#include "thermistor.h"
#include "vdiv.h"

#include <math.h>

#define T_KELVIN                    273.15 // 0C in Kelvin

float rt_calc(int voltage) {
    float R = calc_resistance(voltage, RT_R_PD); // calculate resistance
    float T = 1 / (log(R / RT_R0) / RT_B + 1 / (RT_t0 + T_KELVIN));
        // NOTE: see 5.2D task submission for explanation
    return T - T_KELVIN;
}