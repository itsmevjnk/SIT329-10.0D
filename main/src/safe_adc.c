#include "safe_adc.h"

#include <esp_log.h>
#include <esp_check.h>
#include <esp_adc/adc_cali.h> // calibration driver
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>

#include <freertos/semphr.h>

static adc_cali_handle_t adc_calib; // calibration data
static adc_oneshot_unit_handle_t adc_unit; // ADC unit handle

static StaticSemaphore_t adc_mutex_buf; // backing memory for adc_mutex
static SemaphoreHandle_t adc_mutex;

#define TAG                                 "adc"

void adc_init() {
    /* initalise ADC unit */
    adc_oneshot_unit_init_cfg_t unit_config = {
        /* unit_id */ ADC_UNIT_1, // ADC1
        /* clk_src */ 0, // default
        /* ulp_mode */ ADC_ULP_MODE_DISABLE
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_config, &adc_unit));
    
    /* retrieve calibration */
    adc_cali_line_fitting_config_t calib_config = {
        /* unit_id */ ADC_UNIT_1, // ADC1 (GPIO 32-39)
        /* atten */ ADC_ATTEN_DB_12, // max attenuation for max voltage range
        /* bitwidth */ ADC_BITWIDTH_DEFAULT

    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(
        &calib_config, &adc_calib
    ));

    adc_mutex = xSemaphoreCreateMutexStatic(&adc_mutex_buf);
}

void adc_init_channel(adc_channel_t channel) {
    adc_oneshot_chan_cfg_t config = {
        ADC_ATTEN_DB_12, ADC_BITWIDTH_DEFAULT
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_unit, channel, &config));
}

esp_err_t adc_read(adc_channel_t channel, int *voltage, TickType_t max_wait) {
    if (!adc_unit || !adc_calib || !voltage || !adc_mutex)
        return ESP_ERR_INVALID_STATE; // not initialised yet

    if (!xSemaphoreTake(adc_mutex, max_wait))
        return ESP_ERR_TIMEOUT; // mutex timeout

    int raw; // raw ADC output
    if (adc_oneshot_read(adc_unit, channel, &raw) != ESP_OK) {
        ESP_LOGE(TAG, "cannot read from channel %u", channel);
        xSemaphoreGive(adc_mutex);
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (adc_cali_raw_to_voltage(adc_calib, raw, voltage) != ESP_OK) {
        ESP_LOGE(TAG, "cannot convert raw ADC value (%d) to voltage", raw);
        xSemaphoreGive(adc_mutex);
        return ESP_ERR_INVALID_RESPONSE;
    }

    xSemaphoreGive(adc_mutex);
    return ESP_OK;
}