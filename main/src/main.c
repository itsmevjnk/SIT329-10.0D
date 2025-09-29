#include <stdio.h>

#include <esp_log.h>

#include "safe_adc.h"
#include "fsr.h"
#include "thermistor.h"

#define TAG                 "main" // log tag

void app_main(void)
{
    ESP_LOGI(TAG, "Hello, World!"); // TODO

    adc_init();
    fsr_init();
    rt_init();

    while (true) {
        vTaskDelay(1); // so we can keep watchdog happy
    }
}