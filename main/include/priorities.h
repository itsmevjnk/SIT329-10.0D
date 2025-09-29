#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>

#define MIN_PRIORITY                            (tskIDLE_PRIORITY + 1)
#define MAX_PRIORITY                            (configMAX_PRIORITIES - 1)
