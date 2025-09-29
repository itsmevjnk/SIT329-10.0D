#include "fsr.h"
#include "vdiv.h" // voltage divider support

static const float fsr_curve_R[] = { FSR_RESISTANCES };
static const float fsr_curve_F[] = { FSR_FORCES };

float fsr_calc(int voltage) {
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