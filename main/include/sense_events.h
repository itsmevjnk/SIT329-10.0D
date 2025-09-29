#pragma once

#include <stddef.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>

/* sense event bits */
#define SE_TEMP_UPDATE                          (1 << 0)
// NOTE: UI-facing temperature events are to be handled on frontend

extern EventGroupHandle_t se_events; // shared event group for sensing events

/*
 * void se_init()
 *  Initialises the sensing event group.
 *  Inputs: None.
 *  Output: None.
 */
void se_init();