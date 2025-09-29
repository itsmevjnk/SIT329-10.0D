#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <freertos/FreeRTOS.h>
#include <hal/adc_types.h>

// NOTE: only ADC0 is supported for now

/*
 * void adc_init()
 *  Initialises the ESP32-C3's ADC peripheral in one-shot mode, as well as its
 *  calibration profiles.
 *  Inputs: None.
 *  Output: None.
 */
void adc_init();

/*
 * esp_err_t adc_read(adc_channel_t channel, int *voltage, TickType_t max_wait)
 *  Attempts to read the voltage of a specified ADC0 channel. This function is
 *  essentially a thread-safe wrapper for the ESP32 ADC server, allowing the
 *  ADC channels to be read by multiple tasks.
 *  Inputs:
 *   - channel  : The ADC0 channel number (0-4 for GPIO0-4).
 *   - voltage  : Pointer to the voltage output (in millivolts). This must be
 *                non-null.
 *   - max_wait : The maximum mutex acquisition waiting duration in ticks.
 *  Output: ESP_OK on success.
 */
esp_err_t adc_read(adc_channel_t channel, int *voltage, TickType_t max_wait);