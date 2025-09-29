#pragma once

#include <stddef.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>

/* sense event bits */
#define SE_TEMP_UPDATE                          (1 << 0)
// NOTE: UI-facing temperature events are to be handled on frontend
#define SE_OCC_UPDATE                           (1 << 1) // occupancy update
#define SE_HELP                                 (1 << 2) // help signalling

extern EventGroupHandle_t se_events; // shared event group for sensing events

/*
 * void se_init()
 *  Initialises the sensing event group.
 *  Inputs: None.
 *  Output: None.
 */
void se_init();