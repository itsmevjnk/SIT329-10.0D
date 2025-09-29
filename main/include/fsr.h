#pragma once

#include <stddef.h>
#include <stdint.h>

/* FSR force curve - pairs of reference resistances and force values (g) */
#define FSR_RESISTANCES \
    100000, \
    30000, 10000, 6000, 3500, /* interpolated */ 2000, \
    1250, 750, 450, /* all interpolated */ \
    300, 250 /* interpolated */
#define FSR_FORCES \ 
    16, /* interpolated */ \
    20, 50, 100, 250, /* interpolated */ 500, \
    1000, 2000, 4000, \
    7000, 10000

#define FSR_MAX_FORCE               10000 // maximum rated force

#define FSR_R_PD                    1000 // pulldown resistance

/*
 * float fsr_calc(int voltage)
 *  Calculates the applied force given the FSR's measurement.
 *  Inputs:
 *   - voltage : The measured voltage across the FSR's pulldown resistor in mV.
 *  Output: The applied force in grams.
 */
float fsr_calc(int voltage);
