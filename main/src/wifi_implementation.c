#include "wifi_implementation.h"
#include "credentials.h"
#include "error_checks.h"
#include "pluto_events.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_netif.h"

static const char *WIFI_TAG = "WIFI";
EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
static esp_netif_t *station_network_interface = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
    int32_t event_id, void *event_data)
{
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(WIFI_TAG, "Connecting to WiFi...");
    }   
    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();

        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGW(WIFI_TAG, "WiFi disconnected. Trying to reconnect...");
    }
    else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_init(){
    wifi_event_group = xEventGroupCreate();

    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_ERROR(esp_netif_init(), WIFI_TAG, "esp_netif_init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), WIFI_TAG, "failed to create event loop");

    station_network_interface = esp_netif_create_default_wifi_sta();

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();

    ESP_RETURN_ON_ERROR(esp_wifi_init(&config), WIFI_TAG, "failed to wifi init with config");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL), WIFI_TAG, "failed to register wifi events");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL), WIFI_TAG, "failed to register IP event");

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS
        },
    };

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), WIFI_TAG, "Failed to set wifi mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), WIFI_TAG, "Failed to set config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), WIFI_TAG, "Failed to start wifi");

    return 0;
}

bool wifi_is_connected() {
    EventBits_t bits = xEventGroupGetBits(wifi_event_group);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

esp_err_t wait_for_connection(void) {
    xEventGroupWaitBits(
        wifi_event_group,   // EventGroup
        WIFI_CONNECTED_BIT, // EventBit
        pdFALSE,            // Don't clear bit
        pdTRUE,             // wait for all bits
        pdMS_TO_TICKS(WIFI_MAX_WAIT_MS));     // Wait until true
    
    ESP_LOGI(WIFI_TAG, "Wifi is connected!");

    return ESP_OK;
}

esp_err_t wifi_destroy() {
    return esp_wifi_deinit();
}

void wifi_check_status (void *args) {
    QueueHandle_t queue = (QueueHandle_t)args;
    uint8_t queue_received = 0;

    if (queue != NULL) {
        queue_received = 1;
    }

    while (true && queue_received) {
        if (!wifi_is_connected()) {
            pluto_event_handle_t event = {
                .event_type = EV_WIFI,
                .wifi = false
            };

            xQueueSend(queue, &event, portMAX_DELAY);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    vTaskDelete(NULL);
}