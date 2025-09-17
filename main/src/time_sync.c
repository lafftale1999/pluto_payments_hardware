/* 
    Built using documentation from https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32/api-reference/system/system_time.html
*/

#include "time_sync.h"
#include "error_checks.h"

#include <sys/time.h>
#include <stdbool.h>
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

#define SERVER_TIMEOUT_WAIT_MS  20000
#define TIME_STORAGE_NAMESPACE  "Storage"
#define TIME_STORAGE_NAME       "timespace"


static bool time_inited = false;
const char* TIME_TAG = "TIME";

void time_set_timezone() {
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    tzset();
}

void time_get_current_time(char *buf, size_t buf_size) {
    time_t now = time(NULL);
    struct tm local_time;
    localtime_r(&now, &local_time);

    strftime(buf, buf_size, "%Y-%m-%dT%H:%M:%S", &local_time);
    ESP_LOGI("TIME", "Local time: %s", buf);
}

esp_err_t time_sync_init() {
    if (!time_inited) {
        esp_sntp_config_t config = 
        ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2,
        ESP_SNTP_SERVER_LIST("time.windows.com", "pool.ntp.org" ) );
        ESP_RETURN_ON_ERROR(esp_netif_sntp_init(&config), TIME_TAG, "Failed to initialize sntp");
        time_inited = true;
        return ESP_OK;
    } else {
        ESP_LOGE(TIME_TAG, "SNTP already inited");
        return ESP_ERR_INVALID_STATE;
    }
}

esp_err_t time_sync_deinit() {
    if (time_inited) {
        esp_netif_sntp_deinit();
        time_inited = false;
        return ESP_OK;
    }

    return ESP_ERR_INVALID_STATE;
}

esp_err_t time_update_from_sntp() {
    uint8_t amount_tried = 0;
    uint8_t max_tries = 10;

    esp_err_t err = ESP_FAIL;

    while(amount_tried <= max_tries) {
        err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(SERVER_TIMEOUT_WAIT_MS));

        if (err != ESP_OK) {
            amount_tried++;
            ESP_LOGE(TIME_TAG, "Unable to update time on try %d within %d seconds", amount_tried, (int)(SERVER_TIMEOUT_WAIT_MS / 1000));
            continue;
        }
        break;
    }

    return err;
}

esp_err_t time_update_and_store_in_nvs(void *args) {
    nvs_handle_t nvs_handle = 0;
    esp_err_t err = ESP_FAIL;

    time_sync_init();
    if ((err = time_update_from_sntp()) != ESP_OK) goto exit;

    time_t now;
    time(&now);

    if ((err = nvs_open(TIME_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle)) != ESP_OK) goto exit;
    if ((err = nvs_set_i64(nvs_handle, TIME_STORAGE_NAME, now)) != ESP_OK) goto exit;
    if ((err = nvs_commit(nvs_handle)) != ESP_OK) goto exit;

exit:
    if (nvs_handle != 0) {
        nvs_close(nvs_handle);
    }

    time_sync_deinit();

    if (err != ESP_OK) {
        ESP_LOGE(TIME_TAG, "Error updating time in NVS");
    } else {
        ESP_LOGI(TIME_TAG, "Updated time in NVS");
    }

    return err;
}

esp_err_t time_updated_from_nvs() {
    nvs_handle_t nvs_handle = 0;
    esp_err_t err = ESP_FAIL;

    if ((err = nvs_open(TIME_STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle)) != ESP_OK) {
        ESP_LOGE(TIME_TAG, "Unable to open NVS");
        goto exit;
    }

    int64_t timestamp = 0;

    err = nvs_get_i64(nvs_handle, TIME_STORAGE_NAME, &timestamp);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = time_update_and_store_in_nvs(NULL);
    } else if (err == ESP_OK) {
        struct timeval get_nvs_time;
        get_nvs_time.tv_sec = timestamp;
        settimeofday(&get_nvs_time, NULL);
    }

exit:
    if (nvs_handle != 0) {
        nvs_close(nvs_handle);
    }

    return err;
}