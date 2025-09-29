#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* thermistor parameters */
#define RT_B                        3950 // B constant
#define RT_R0                       10000 // rated resistance
#define RT_t0                       25 // rated temperature (C)
#define RT_R_PD                     10000 // pulldown resistor

/*
 * float rt_calc(int voltage)
 *  Calculates the temperature given the thermistor's measurement.
 *  Inputs:
 *   - voltage : The measured voltage across the thermistor's pulldown resistor
 *               in mV.
 *  Output: The applied force in C.
 */
float rt_calc(int voltage);