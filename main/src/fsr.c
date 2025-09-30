#include "fsr.h"
#include "vdiv.h" // voltage divider support
#include "safe_adc.h"
#include "priorities.h"
#include "sense_events.h"

#include <esp_log.h>

#include <math.h>

#define TAG                         "fsr" // for logging

static const float fsr_curve_R[] = { FSR_RESISTANCES };
static const float fsr_curve_F[] = { FSR_FORCES };

/*
 * static float fsr_calc(int voltage)
 *  Calculates the applied force given the FSR's measurement.
 *  Inputs:
 *   - voltage : The measured voltage across the FSR's pulldown resistor in mV.
 *  Output: The applied force in grams.
 */
static float fsr_calc(int voltage) {
    float R = calc_resistance(voltage, FSR_R_PD); // calculate resistance

    if (R > fsr_curve_R[0]) return 0; // measured resistance > max

    size_t seg = 0; // curve segment index - [seg] to [seg + 1]
    for (; seg < sizeof(fsr_curve_R) / sizeof(float) - 1; seg++) {
        if (R <= fsr_curve_R[seg] && R >= fsr_curve_R[seg + 1]) break;
    }

    /* find segment's slope and intercept */
    float slope = 
            (fsr_curve_F[seg + 1] - fsr_curve_F[seg])
        /   (fsr_curve_R[seg + 1] - fsr_curve_R[seg]);
    float intercept = fsr_curve_F[seg] - fsr_curve_R[seg] * slope;

    float F = slope * R + intercept; // i.e. linearly map R to F
    if (F > FSR_MAX_FORCE) F = FSR_MAX_FORCE; // clamp force

    return F;
}

static StaticTask_t fsr_tap_task_buf;
#define STACK_SIZE                          2048
static StackType_t fsr_tap_task_stack[STACK_SIZE];

#define FSR_TAP_QUEUE_LEN                   16
static QueueHandle_t fsr_tap_queue; // for passing tap tickstamps
static TickType_t fsr_tap_queue_stor[FSR_TAP_QUEUE_LEN];
static StaticQueue_t fsr_tap_queue_buf;

/*
 * static void fsr_tap_task(void *parameter)
 *  Task function for the FSR tap aggregation task.
 *  Inputs:
 *   - parameter: Parameter from xTaskCreateStatic - ignored.
 *  Output: None.
 */
static void fsr_tap_task(void *parameter) {
    TickType_t tap_stamps[FSR_NUM_TAPS];
    size_t count = 0;

    while (true) {
        TickType_t stamp;
        if (xQueueReceive(fsr_tap_queue, &stamp, portMAX_DELAY)) {
            /* received tickstamp */
            tap_stamps[count % FSR_NUM_TAPS] = stamp;
            count++;

            if (count >= FSR_NUM_TAPS) {
                TickType_t first = tap_stamps[
                    (count - FSR_NUM_TAPS) % FSR_NUM_TAPS
                ];
                if (stamp - first <= pdMS_TO_TICKS(FSR_TAP_DURATION)) {
                    ESP_LOGI(
                        TAG, "%d taps registered in %d ms - triggering signal",
                        FSR_NUM_TAPS, pdTICKS_TO_MS(stamp - first)
                    );
                    xEventGroupSetBits(se_events, SE_HELP);
                    count = 0; // might be a good iea to do this anyway
                }
            }
        }
    }
}

static float fsr_avg_force; // average recorded force
static TickType_t fsr_last_tap = 0; // tickstamp of last tap - for debouncing

/* task support structures */
static StaticTask_t fsr_task_buf; // TCB
static StackType_t fsr_task_stack[STACK_SIZE];

/*
 * static void fsr_task(void *parameter)
 *  Task function for the FSR sensing task.
 *  Inputs:
 *   - parameter: Parameter from xTaskCreateStatic - ignored.
 *  Output: None.
 */
static void fsr_task(void *parameter) {
    while (true) {
        float force = fsr_read(portMAX_DELAY);
        fsr_avg_force = // exponential moving average
            FSR_AVG_FACTOR * force + (1 - FSR_AVG_FACTOR) * fsr_avg_force;

        if (force - fsr_avg_force >= FSR_TAP_THRESHOLD) {
            TickType_t now = xTaskGetTickCount();
            if (now - fsr_last_tap >= pdMS_TO_TICKS(FSR_TAP_DEBOUNCE)) {
                ESP_LOGI(TAG, "tap detected");
                fsr_last_tap = now;
                xQueueSend(fsr_tap_queue, &now, 0); // so we don't get stuck
            }
        }

        vTaskDelay(pdMS_TO_TICKS(FSR_INTERVAL));
    }
}

static StaticTimer_t fsr_occ_timer_buf; // buffer for occupancy check timer

bool fsr_occupancy;

/*
 * static void fsr_occ_callback(TimerHandle_t timer)
 *  Callback function for checking bed occupancy.
 *  Inputs:
 *   - timer : The timer that triggered this callback.
 *  Output: None.
 */
static void fsr_occ_callback(TimerHandle_t timer) {
    (void) timer;
    bool occupancy = fsr_avg_force >= FSR_OCC_THRESHOLD;
    ESP_LOGI(
        TAG, "average force: %.2f g (occupancy: %d)",
        fsr_avg_force, occupancy ? 1 : 0
    );
    if (occupancy != fsr_occupancy) {
        fsr_occupancy = occupancy;
        xEventGroupSetBits(se_events, SE_OCC_UPDATE);
    }
}

void fsr_init() {
    adc_init_channel(FSR_PIN_CHANNEL);
    fsr_avg_force = fsr_read(portMAX_DELAY); // initialise average force
    fsr_occupancy = fsr_avg_force >= FSR_OCC_THRESHOLD; // and also occupancy

    fsr_tap_queue = xQueueCreateStatic(
        FSR_TAP_QUEUE_LEN, sizeof(TickType_t),
        (uint8_t *)&fsr_tap_queue_stor, &fsr_tap_queue_buf
    );

    xTaskCreateStatic(
        fsr_task, "fsr", STACK_SIZE, NULL, MAX_PRIORITY,
        fsr_task_stack, &fsr_task_buf
    ); // create sensing task

    xTaskCreateStatic(
        fsr_tap_task, "fsr_tap", STACK_SIZE, NULL, MAX_PRIORITY - 1,
        fsr_tap_task_stack, &fsr_tap_task_buf
    ); // create tap aggregation task

    TimerHandle_t timer = xTimerCreateStatic(
        "fsr_occ", pdMS_TO_TICKS(1000 * 60 * FSR_OCC_PERIOD), pdTRUE,
        NULL, fsr_occ_callback, &fsr_occ_timer_buf
    ); // create occupancy check timer
    xTimerStart(timer, portMAX_DELAY);
}

float fsr_read(TickType_t max_wait) {
    int voltage;
    if (adc_read(FSR_PIN_CHANNEL, &voltage, max_wait) != ESP_OK) return NAN;
    return fsr_calc(voltage);
}