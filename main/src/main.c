#include <stdio.h>

#include <esp_log.h>

#include "sense_events.h"
#include "safe_adc.h"
#include "fsr.h"
#include "thermistor.h"
#include "webserver.h"

#define TAG                 "main" // log tag

void app_main(void)
{
    ESP_LOGI(TAG, "Hello, World!"); // TODO

    se_init();
    adc_init();
    fsr_init();
    rt_init();
    web_init();

    while (true) {
        vTaskDelay(1); // so we can keep watchdog happy
    }
}