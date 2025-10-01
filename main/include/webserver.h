#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* WiFi credentials - we may need to make this configurable */
#define WIFI_SSID                   "BedTest"
#define WIFI_PASSWORD               "12345678"

/*
 * void web_init()
 *  Initialises the web server.
 *  Inputs: None.
 *  Output: None.
 */
void web_init();
