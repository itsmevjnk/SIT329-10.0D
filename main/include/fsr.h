#pragma once

#include <stddef.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>

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

/* pin config */
#define FSR_PIN_CHANNEL             ADC_CHANNEL_7 // ADC channel of sense pin
    // GPIO 35 - ADC1 channel 7

#define FSR_INTERVAL                20 // interval between FSR readings (in ms)
#define FSR_AVG_FACTOR              0.25 // alpha factor for exp. moving avg
#define FSR_OCC_THRESHOLD           500 // threshold for occupancy
#define FSR_OCC_PERIOD              1 // period (in mins) between occ. checks
#define FSR_TAP_THRESHOLD           2000 // threshold for mat tapping
#define FSR_TAP_DEBOUNCE            150 // debounce duration (ms) between taps

#define FSR_NUM_TAPS                5 // number of taps for signal trigger
#define FSR_TAP_DURATION            2000 // duration (ms) for tap to be reg'd

extern bool fsr_occupancy; // occupancy status

/*
 * void fsr_init()
 *  Initialises FSR sensing.
 *  Inputs: None.
 *  Output: None.
 */
void fsr_init();

/*
 * float fsr_read(TickType_t max_wait)
 *  Reads the current force measurement from the FSR.
 *  Inputs:
 *   - max_wait : Max duration (in ticks) to wait for ADC mutex acquisition.
 *  Output: The force measurement in grams, or NAN if reading failed.
 */
float fsr_read(TickType_t max_wait);
