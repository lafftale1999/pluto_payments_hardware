#include <stdio.h>
#include "pluto_system.h"
#include "time_sync.h"
#include "wifi_implementation.h"

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

const char* MAIN_TAG = "MAIN";

int app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(5000));

    // initialize NVS
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    pluto_system_handle_t pluto = NULL;
    ESP_ERROR_CHECK(pluto_system_init(&pluto));

    ESP_ERROR_CHECK(time_update_and_store_in_nvs(NULL));
    time_set_timezone();

    pluto_run(pluto);
    
    return 0;
}