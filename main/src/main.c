#include <stdio.h>

#include <esp_log.h>

#include "safe_adc.h"
#include "fsr.h"

#define TAG                 "main" // log tag

void app_main(void)
{
    ESP_LOGI(TAG, "Hello, World!"); // TODO

    adc_init();

    while (true) {
        int voltage;
        ESP_ERROR_CHECK(adc_read(ADC_CHANNEL_0, &voltage, portMAX_DELAY));
        float force = fsr_calc(voltage);
        printf("%f\n", force); // can be plotted using Arduino serial plotter
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}