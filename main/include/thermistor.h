#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <freertos/FreeRTOS.h>

/* thermistor parameters */
#define RT_B                        3950 // B constant
#define RT_R0                       10000 // rated resistance
#define RT_t0                       25 // rated temperature (C)
#define RT_R_PD                     10000 // pulldown resistor

/* pin configuration */
#define RT_PIN_CHANNEL              ADC_CHANNEL_1 // ADC channel of sense pin
#define RT_LED_PIN                  10 // bedside LED for high temp alert

#define RT_SENSE_PERIOD             5 // period (in mins) to sense temperature
#define RT_LED_THRESHOLD            38 // threshold for high temp LED alert
#define RT_LED_PERIOD               1000 // high temp LED blink period

/* temperature history */
#define RT_HISTORY_LEN              (24 * 60 / RT_SENSE_PERIOD) // entry count
extern float rt_history[RT_HISTORY_LEN]; // initially filled with NAN
extern float *rt_latest_temp; // latest reading (last element in history)

/*
 * void rt_init()
 *  Initialises thermistor sensing.
 *  Inputs: None.
 *  Output: None.
 */
void rt_init();

/*
 * float rt_read(TickType_t max_wait)
 *  Reads the current temperature measurement from the thermistor.
 *  Inputs:
 *   - max_wait : Max duration (in ticks) to wait for ADC mutex acquisition.
 *  Output: The temperature in C, or NAN if reading failed.
 */
float rt_read(TickType_t max_wait);