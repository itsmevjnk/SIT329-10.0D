#include "webserver.h"

#include <esp_log.h>
#include <esp_check.h>
#include <esp_http_server.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include "thermistor.h"
#include "fsr.h"
#include "sense_events.h"
#include "priorities.h"

#include <math.h>

#define TAG                                 "web"

static httpd_handle_t web_handle;

/* static data definition */
struct web_static {
    const void *start; // pointer to start of buffer
    const void *end; // pointer to end of buffer
    const char *mime; // MIME type
};

/*
 * static esp_err_t web_serve_static(httpd_req_t *req)
 *  Serves static data provided in req->user_ctx.
 *  Inputs:
 *   - req : The request object from the HTTPD server, with user_ctx set to
 *           a struct web_static object containing the file to be served.
 *  Output: ESP_OK on success, otherwise a corresponding error code.
 */
esp_err_t web_serve_static(httpd_req_t *req) {
    struct web_static* data = (struct web_static *)req->user_ctx;
    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_ARG, TAG, "user_ctx is null");

    if (data->mime) ESP_RETURN_ON_ERROR(
        httpd_resp_set_hdr(req, "Content-Type", data->mime),
        TAG, "cannot set Content-Type header"
    );
    ESP_RETURN_ON_ERROR(
        httpd_resp_send(
            req, (const char*)data->start,
            (size_t)data->end - (size_t)data->start
        ),
        TAG, "cannot send response"
    );
    
    return ESP_OK;
}

extern const uint8_t chart_min_js_start[]
    asm("_binary_chart_umd_min_js_start");
extern const uint8_t chart_min_js_end[]
    asm("_binary_chart_umd_min_js_end");
static const httpd_uri_t web_get_chart_min_js = {
    "/chart.umd.min.js", HTTP_GET,
    web_serve_static,
    (const void *)&(struct web_static){
        chart_min_js_start, chart_min_js_end,
        "application/javascript"
    }
};

extern const uint8_t index_htm_start[] asm("_binary_index_htm_start");
extern const uint8_t index_htm_end[] asm("_binary_index_htm_end");
static const httpd_uri_t web_get_index_htm = {
    "/index.htm", HTTP_GET,
    web_serve_static,
    (const void *)&(struct web_static){
        index_htm_start, index_htm_end,
        "text/html"
    }
};
static const httpd_uri_t web_get_root = {
    "/", HTTP_GET,
    web_serve_static,
    (const void *)&(struct web_static){
        index_htm_start, index_htm_end,
        "text/html"
    }
};

extern const uint8_t alert_mp3_start[] asm("_binary_alert_mp3_start");
extern const uint8_t alert_mp3_end[] asm("_binary_alert_mp3_end");
static const httpd_uri_t web_get_alert_mp3 = {
    "/alert.mp3", HTTP_GET,
    web_serve_static,
    (const void *)&(struct web_static){
        alert_mp3_start, alert_mp3_end,
        "audio/mpeg"
    }
};

/*
 * static void web_ws_send_all_temps(void *arg)
 *  Sends all logged temperatures to the specified client.
 *  Inputs:
 *   - arg : The client's file descriptor.
 *  Output: None.
 */
static void web_ws_send_all_temps(void *arg) {
    httpd_ws_frame_t frame; memset(&frame, 0, sizeof(httpd_ws_frame_t));
    frame.payload = malloc(7 * RT_HISTORY_LEN + 3);
        // 7 chars per element (incl. comma) + 2 byte header + null termination
    assert(frame.payload);
    frame.type = HTTPD_WS_TYPE_TEXT;
    int fd = (int)arg;

    /* prepare payload */
    frame.payload[0] = 'T'; frame.payload[1] = ':';
    // while (!xSemaphoreTake(rt_data_mutex, portMAX_DELAY));
    frame.len = 2;
    for (size_t i = 0; i < RT_HISTORY_LEN; i++) {
        if (isnan(rt_history[i])) continue; // skip NaN entries in the beginning
        frame.len += // excluding null termination
            sprintf(
                (char *)&frame.payload[frame.len], "%3.2f,", rt_history[i]
            );
    }
    // xSemaphoreGive(rt_data_mutex);
    if (frame.len > 2) frame.len--; // cut off the final comma

    esp_err_t ret = httpd_ws_send_frame_async(web_handle, fd, &frame);
    free(frame.payload);
    if (ret != ESP_OK)
        ESP_LOGE(
            TAG, "cannot send WebSocket data to client fd %d (%s)",
            fd, esp_err_to_name(ret)
        );
    else ESP_LOGD(TAG, "sent WebSocket data to client %d", fd);
}

/*
 * static void web_ws_send_last_temp(void *arg)
 *  Sends the newest recorded temperature to the specified client.
 *  Inputs:
 *   - arg : The client's file descriptor.
 *  Output: None.
 */
static void web_ws_send_last_temp(void *arg) {
    char buf[2 + 6 + 1]; // 2 byte header + 6 chars (max) + null termination
    httpd_ws_frame_t frame; memset(&frame, 0, sizeof(httpd_ws_frame_t));
    frame.payload = (uint8_t *)buf;
    frame.type = HTTPD_WS_TYPE_TEXT;
    int fd = (int)arg;

    volatile float temp = *rt_latest_temp; // atomic
    frame.len = sprintf(buf, "t:%3.2f", temp);

    esp_err_t ret = httpd_ws_send_frame_async(web_handle, fd, &frame);
    if (ret != ESP_OK)
        ESP_LOGE(
            TAG, "cannot send WebSocket data to client fd %d (%s)",
            fd, esp_err_to_name(ret)
        );
    else ESP_LOGD(TAG, "sent WebSocket data to client %d", fd);
}

/*
 * static void web_ws_send_occupancy(void *arg)
 *  Sends occupancy status (0 = unoccupied, 1 = occupied) to the specified
 *  client.
 *  Inputs:
 *   - arg : The client's file descriptor.
 *  Output: None.
 */
static void web_ws_send_occupancy(void *arg) {
    char buf[2 + 1 + 1] = "o:\0"; // 2 byte header + 1 char + null termination
    httpd_ws_frame_t frame; memset(&frame, 0, sizeof(httpd_ws_frame_t));
    frame.payload = (uint8_t *)buf;
    frame.type = HTTPD_WS_TYPE_TEXT;
    int fd = (int)arg;

    frame.payload[2] = fsr_occupancy ? '1' : '0';
    frame.len = 3;

    esp_err_t ret = httpd_ws_send_frame_async(web_handle, fd, &frame);
    if (ret != ESP_OK)
        ESP_LOGE(
            TAG, "cannot send WebSocket data to client fd %d (%s)",
            fd, esp_err_to_name(ret)
        );
    else ESP_LOGD(TAG, "sent WebSocket data to client %d", fd);
}

static bool web_help = false; // set when help is signalled

/*
 * static void web_ws_send_help(void *arg)
 *  Sends help request status (0/1) to the specified client.
 *  Inputs:
 *   - arg : The client's file descriptor.
 *  Output: None.
 */
static void web_ws_send_help(void *arg) {
    char buf[2 + 1 + 1] = "h:\0"; // 2 byte header + 1 char + null termination
    httpd_ws_frame_t frame; memset(&frame, 0, sizeof(httpd_ws_frame_t));
    frame.payload = (uint8_t *)buf;
    frame.type = HTTPD_WS_TYPE_TEXT;
    int fd = (int)arg;

    frame.payload[2] = web_help ? '1' : '0';
    frame.len = 3;

    esp_err_t ret = httpd_ws_send_frame_async(web_handle, fd, &frame);
    if (ret != ESP_OK)
        ESP_LOGE(
            TAG, "cannot send WebSocket data to client fd %d (%s)",
            fd, esp_err_to_name(ret)
        );
    else ESP_LOGD(TAG, "sent WebSocket data to client %d", fd);
}

/*
 * static esp_err_t web_ws_handler(httpd_req_t *req)
 *  Handler for incoming WebSocket clients. Sends initial data to connecting
 *  clients.
 *  Inputs:
 *   - req : The client's HTTP request.
 *  Output: ESP_OK.
 */
static esp_err_t web_ws_handler(httpd_req_t *req) {
    int fd = httpd_req_to_sockfd(req);

    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "new client connected to WebSocket with fd %d", fd);
        return ESP_OK;
    }

    /* we assume any request past this point to be WebSocket */

    httpd_ws_frame_t frame; memset(&frame, 0, sizeof(httpd_ws_frame_t));
    ESP_ERROR_CHECK(httpd_ws_recv_frame(req, &frame, 0));

    /* regardless of the actual data, we'll send the complete data over */
    web_ws_send_all_temps((void *)fd);
    web_ws_send_occupancy((void *)fd);
    web_ws_send_help((void *)fd);

    return ESP_OK;
}

static const httpd_uri_t web_ws = {
    .uri = "/ws", .method = HTTP_GET,
    .handler = web_ws_handler,
    .user_ctx = NULL,
    .is_websocket = true
};


/*
 * static void web_ws_send_all(httpd_work_fn_t func)
 *  Stages the provided function (which can be and of the web_ws_send_x
 *  functions above) to execute for all connected clients. This can be used to
 *  broadcast a message to all clients.
 *  Inputs:
 *   - func : The send function to use.
 *  Output: None.
 */
static void web_ws_send_all(httpd_work_fn_t func) {
    /* get all FDs of connected clients */
    int client_fds[CONFIG_LWIP_MAX_LISTENING_TCP];
    size_t fds = CONFIG_LWIP_MAX_LISTENING_TCP;
    ESP_ERROR_CHECK(httpd_get_client_list(web_handle, &fds, client_fds));

    for (size_t i = 0; i < fds; i++) {
        httpd_ws_client_info_t info = 
            httpd_ws_get_fd_info(web_handle, client_fds[i]);
        if (info == HTTPD_WS_CLIENT_WEBSOCKET) {
            ESP_LOGD(
                TAG, "staging WebSocket transmission for client fd %d",
                client_fds[i]
            );
            ESP_ERROR_CHECK(
                httpd_queue_work(web_handle, func, (void*)client_fds[i])
            );
        }
    }
}

/*
 * static esp_err_t web_clear_help(httpd_req_t *req)
 *  Clears the active help request.
 *  Inputs:
 *   - req : Request data from HTTPD.
 *  Output: ESP_OK on success.
 */
static esp_err_t web_clear_help(httpd_req_t *req) {
    web_help = false;
    ESP_LOGI(TAG, "help request cleared");
    web_ws_send_all(web_ws_send_help); // broadcast new help status
    return httpd_resp_send(req, NULL, 0);
}

static const httpd_uri_t web_post_clear = {
    "/clear", HTTP_POST,
    web_clear_help
};

/* all URI handlers */
const httpd_uri_t *web_handlers[] = {
    &web_get_index_htm, &web_get_root,
    &web_get_alert_mp3, &web_get_chart_min_js, 
    &web_ws, &web_post_clear
};

/*
 * static void web_wifi_event_handler(void *arg, esp_event_base_t event_base,
 *                                    int32_t event_id, void *event_data)
 *  Handler for networking (WiFi/IP stack) events, including WiFi station
 *  starts, network disconnections, and IP address acquisition.
 *  Inputs:
 *   - arg        : Arbitrary argument passed from ESP-IDF; ignored.
 *   - event_base : The event's source (WiFi or IP stack).
 *   - event_id   : The event's identifier.
 *   - event_data : Arbitrary data supplied along with the event.
 *  Output: None.
 */
static void web_wifi_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data){
    if (
        event_base == WIFI_EVENT && (
            event_id == WIFI_EVENT_STA_START
            || event_id == WIFI_EVENT_STA_DISCONNECTED
        )
    ) {
        ESP_LOGI(TAG, "connecting/reconnecting to WiFi");
        ESP_ERROR_CHECK(esp_wifi_connect()); // connect/reconnect
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP address: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

/*
 * static void web_event_task(void *parameter)
 *  Task function for the webserver's event handling functionality.
 *  Inputs:
 *   - parameter: Parameter from xTaskCreateStatic - ignored.
 *  Output: None.
 */
static void web_event_task(void *parameter) {
    (void) parameter;

    while (true) {
        EventBits_t events = xEventGroupWaitBits(
            se_events, SE_TEMP_UPDATE | SE_OCC_UPDATE | SE_HELP,
            pdTRUE, pdFALSE, // wait for any of the above events + clr on exit
            portMAX_DELAY
        );
        if (events & SE_TEMP_UPDATE) { // temperature update
            web_ws_send_all(web_ws_send_last_temp);   
        }
        if (events & SE_OCC_UPDATE) { // occupancy update
            web_ws_send_all(web_ws_send_occupancy);   
        }
        if (events & SE_HELP) { // help signalled
            web_help = true;
            web_ws_send_all(web_ws_send_help);
        }
    }
}

static StaticTask_t web_task_buf; // TCB
#define STACK_SIZE                          2048
static StackType_t web_task_stack[STACK_SIZE];

void web_init() {    
    /* initialise NVS */
    esp_err_t ret = nvs_flash_init();
    if (
            ret == ESP_ERR_NVS_NO_FREE_PAGES
        ||  ret == ESP_ERR_NVS_NEW_VERSION_FOUND
    ) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* initialise networking */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());    
    esp_netif_create_default_wifi_sta();
    
    /* initialise WiFi */
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &web_wifi_event_handler,
            NULL, &instance_any_id
        )
    );
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, &web_wifi_event_handler,
            NULL, &instance_got_ip
        )
    );
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t sta_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,            
            .threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    
    ESP_ERROR_CHECK(esp_wifi_start());

    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    httpd_config.lru_purge_enable = true;
    httpd_config.max_uri_handlers = 
        sizeof(web_handlers) / sizeof(httpd_uri_t*);
    ESP_ERROR_CHECK(httpd_start(&web_handle, &httpd_config));

    /* register URI handlers */
    for (size_t i = 0; i < httpd_config.max_uri_handlers; i++) {
        ESP_ERROR_CHECK(
            httpd_register_uri_handler(web_handle, web_handlers[i])
        );
    }

    /* create task to watch for events */
    xTaskCreateStatic(
        web_event_task, TAG "_event", STACK_SIZE, NULL, MAX_PRIORITY - 2,
        web_task_stack, &web_task_buf
    );
}