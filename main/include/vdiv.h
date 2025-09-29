#pragma once

#include <stddef.h>
#include <stdint.h>

/*
 * static inline float calc_resistance(int VR2, float R2)
 *  Calculates the resistance R1 in a (VCC) -- R1 -- R2 -- (GND) configuration.
 *  This function assumes that VCC = 3.3V.
 *  Inputs:
 *   - VR2 : Voltage across R2 in millivolts.
 *   - R2 : R2 resistance in ohms.
 *  Output: R1 resistance in ohms.
 */
static inline float calc_resistance(int VR2, float R2) {
    // I = VCC / (R1 + R2) = VR2 / R2 => R1 + R2 = R2 * VCC / VR2
    //                                => R1 = R2 * (VCC / VR2 - 1)
    return R2 * (3300.0 / VR2 - 1);
}
