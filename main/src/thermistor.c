#include "thermistor.h"
#include "vdiv.h"
#include "safe_adc.h"
#include "priorities.h"

#include <esp_log.h>

#include <math.h>
#include <string.h>

#include <driver/gpio.h>

#define TAG                         "thermistor" // for logging

#define T_KELVIN                    273.15 // 0C in Kelvin

/*
 * static float rt_calc(int voltage)
 *  Calculates the temperature given the thermistor's measurement.
 *  Inputs:
 *   - voltage : The measured voltage across the thermistor's pulldown resistor
 *               in mV.
 *  Output: The applied force in C.
 */
static float rt_calc(int voltage) {
    float R = calc_resistance(voltage, RT_R_PD); // calculate resistance
    float T = 1 / (log(R / RT_R0) / RT_B + 1 / (RT_t0 + T_KELVIN));
        // NOTE: see 5.2D task submission for explanation
    return T - T_KELVIN;
}

float rt_history[RT_HISTORY_LEN];
float *rt_latest_temp = &rt_history[RT_HISTORY_LEN - 1];

/* LED timer */
static TimerHandle_t rt_led_timer;
static StaticTimer_t rt_led_timer_buf;

static volatile bool rt_led_state = false;

/*
 * static void rt_led_callback(TimerHandle_t timer)
 *  Callback function to be called on each LED state toggle (half cycle).
 *  Inputs:
 *   - timer : The timer that triggered this callback.
 *  Output: None.
 */
static void rt_led_callback(TimerHandle_t timer) {
    (void) timer;
    rt_led_state = !rt_led_state;
    gpio_set_level(RT_LED_PIN, (rt_led_state) ? 1 : 0);
}

/* task support structures */
static StaticTask_t rt_task_buf; // TCB
#define STACK_SIZE                          2048
static StackType_t rt_task_stack[STACK_SIZE];

/*
 * static void rt_task(void *parameter)
 *  Task function for the thermistor sensing task.
 *  Inputs:
 *   - parameter: Parameter from xTaskCreateStatic - ignored.
 *  Output: None.
 */
static void rt_task(void *parameter) {
    while (true) {
        float temp = rt_read(portMAX_DELAY); // read temperature
        ESP_LOGI(TAG, "temperature: %.2f C", temp);
        
        /* log to history */
        memmove(
            rt_history, &rt_history[1], (RT_HISTORY_LEN - 1) * sizeof(float)
        ); // shift up by 1 element
        *rt_latest_temp = temp;

        /* start/stop LED blinking */
        if (temp >= RT_LED_THRESHOLD) {
            if (!xTimerIsTimerActive(rt_led_timer)) {// start timer
                gpio_set_level(RT_LED_PIN, 1); rt_led_state = true;
                assert(xTimerStart(rt_led_timer, portMAX_DELAY) == pdPASS);
                ESP_LOGI(TAG, "activated LED alarm");
            }
        } else if (xTimerIsTimerActive(rt_led_timer)) { // stop timer
            assert(xTimerStop(rt_led_timer, portMAX_DELAY) == pdPASS);
            gpio_set_level(RT_LED_PIN, 0); rt_led_state = false;
            ESP_LOGI(TAG, "deactivated LED alarm");
        }

        vTaskDelay(pdMS_TO_TICKS(1000 * 60 * RT_SENSE_PERIOD));
    }
}

void rt_init() {
    adc_init_channel(RT_PIN_CHANNEL);
    
    for (size_t i = 0; i < RT_HISTORY_LEN; i++) rt_history[i] = NAN;
    
    /* configure LED pin */
    gpio_config_t config = {
        (1ULL << RT_LED_PIN),
        GPIO_MODE_OUTPUT,
        GPIO_PULLUP_DISABLE, GPIO_PULLDOWN_DISABLE, GPIO_INTR_DISABLE
    }; // plain output
    gpio_config(&config);

    /* configure LED timer */
    rt_led_timer = xTimerCreateStatic(
        "thermistor_led",
        pdMS_TO_TICKS(RT_LED_PERIOD / 2), // toggle period
        pdTRUE, NULL, rt_led_callback, &rt_led_timer_buf
    );
    assert(rt_led_timer);
    xTimerStop(rt_led_timer, portMAX_DELAY);

    xTaskCreateStatic(
        rt_task, "thermistor", STACK_SIZE, NULL, MAX_PRIORITY,
        rt_task_stack, &rt_task_buf
    ); // create sensing task
}

float rt_read(TickType_t max_wait) {
    int voltage;
    if (adc_read(RT_PIN_CHANNEL, &voltage, max_wait) != ESP_OK) return NAN;
    return rt_calc(voltage);
}