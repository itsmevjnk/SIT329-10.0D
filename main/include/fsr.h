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
#define FSR_PIN_CHANNEL             ADC_CHANNEL_0 // ADC channel of sense pin

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
