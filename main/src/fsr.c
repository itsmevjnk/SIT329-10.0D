#include "fsr.h"
#include "vdiv.h" // voltage divider support

#include "safe_adc.h"

#include <math.h>

static const float fsr_curve_R[] = { FSR_RESISTANCES };
static const float fsr_curve_F[] = { FSR_FORCES };

/*
 * static float fsr_calc(int voltage)
 *  Calculates the applied force given the FSR's measurement.
 *  Inputs:
 *   - voltage : The measured voltage across the FSR's pulldown resistor in mV.
 *  Output: The applied force in grams.
 */
static float fsr_calc(int voltage) {
    float R = calc_resistance(voltage, FSR_R_PD); // calculate resistance

    if (R > fsr_curve_R[0]) return 0; // measured resistance > max

    size_t seg = 0; // curve segment index - [seg] to [seg + 1]
    for (; seg < sizeof(fsr_curve_R) / sizeof(float) - 1; seg++) {
        if (R <= fsr_curve_R[seg] && R >= fsr_curve_R[seg + 1]) break;
    }

    /* find segment's slope and intercept */
    float slope = 
            (fsr_curve_F[seg + 1] - fsr_curve_F[seg])
        /   (fsr_curve_R[seg + 1] - fsr_curve_R[seg]);
    float intercept = fsr_curve_F[seg] - fsr_curve_R[seg] * slope;

    float F = slope * R + intercept; // i.e. linearly map R to F
    if (F > FSR_MAX_FORCE) F = FSR_MAX_FORCE; // clamp force

    return F;
}

void fsr_init() {
    adc_init_channel(FSR_PIN_CHANNEL);
}

float fsr_read(TickType_t max_wait) {
    int voltage;
    if (adc_read(FSR_PIN_CHANNEL, &voltage, max_wait) != ESP_OK) return NAN;
    return fsr_calc(voltage);
}