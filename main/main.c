#include <stdio.h>
#include "pluto_system.h"

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

const char* MAIN_TAG = "MAIN";

int app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(5000));

    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    pluto_system_handle_t pluto = NULL;
    if (plut_system_init(pluto) != 0) {
        ESP_LOGE(MAIN_TAG, "Unable to initialize alarm system");
    }

    while(true) {
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}