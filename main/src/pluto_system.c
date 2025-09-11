#include "pluto_system.h"
#include "pluto_events.h"
#include "lcd_1602.h"
#include "error_checks.h"
#include "rc522_implementation.h"
#include "keypad_implementation.h"
#include "wifi_implementation.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_err.h"

const char *PLUTO_TAG = "PLUTO_SYSTEM";

typedef struct pluto_payment {
    char payment;

}pluto_payment;

typedef enum pluto_system_state {
    SYS_SLEEPING,
    SYS_WAITING,
    SYS_CREATE_PAYMENT,
    SYS_MAKE_PAYMENT,
    SYS_CHECK_PAYMENT,
    SYS_WIFI_DISCONNECTED,
    SYS_WIFI_CONNECTED
}pluto_system_state;

typedef struct pluto_system {
    QueueHandle_t event_queue;
    rc522_handle_t rc522;
    pluto_system_state current_state;
    pluto_system_state last_state;
    i2c_master_dev_handle_t lcd_i2c;
}pluto_system;

// check payment

// make payment

// create payment

// wakeup

// wifi check task

uint8_t plut_system_init(pluto_system_handle_t *handle) {
    if (handle == NULL || *handle != NULL) {
        ESP_LOGE(PLUTO_TAG, "Handle already initialized");
        return 1;
    }

    // CREATE TEMPORARY HANDLE
    pluto_system_handle_t temp_handle = (pluto_system_handle_t)calloc(1, sizeof(pluto_system));
    if (temp_handle == NULL) {
        ESP_LOGE(PLUTO_TAG, "Failed to allocate memory for structure");
        return 1;
    }

    // CREATE QUEUE
    temp_handle->event_queue = xQueueCreate(10, sizeof(pluto_event_handle_t));
    if (temp_handle->event_queue == NULL) {
        ESP_LOGE(PLUTO_TAG, "Failed to create queue");
        goto exit;
    }

    // START I2C COMMUNICATION
    bool i2c_is_created = false;
    i2c_master_bus_handle_t bus_handle;
    if (i2c_open(&bus_handle, &temp_handle->lcd_i2c, DEVICE_ADDRESS) != 0) {
        ESP_LOGE(PLUTO_TAG, "Failed to open i2c communication");
        goto exit;
    } else {
        i2c_is_created = true;
    }

    // INITIALIZE LCD SCREEN
    if (lcd_1602_init(temp_handle->lcd_i2c) != 0) {
        ESP_LOGE(PLUTO_TAG, "Failed to initialize lcd screen");
        goto exit;
    }

    // INITIALIZE KEYBOARD
    if (_4x4_matrix_init(KEYPAD_ROW_PINS, KEYPAD_COL_PINS) != 0) {
        ESP_LOGE(PLUTO_TAG, "Failed to initialize keyboard");
        goto exit;
    }

    // INITIALIZE RC522
    if (rc522_init(&temp_handle->rc522, temp_handle->event_queue) != 0) {
        ESP_LOGE(PLUTO_TAG, "Failed to initialize RC522 scanner");
        goto exit;
    }

    // INITIALIZE WIFI
    bool wifi_is_init = false;
    lcd_1602_send_string(temp_handle->lcd_i2c, "Connecting to\ninternet...");
    if (wifi_init() != 0) {
        ESP_LOGE(PLUTO_TAG, "Failed to initialize Wi-fi");
        goto exit;
    } else {
        wifi_is_init = true;
    }

    // WAIT FOR INTERNET CONNECTION
    if (wait_for_connection() != 0) {
        ESP_LOGE(PLUTO_TAG, "Failed to connect to Wi-fi");
        goto exit;
    }

    *handle = temp_handle;

    return 0;

exit:
    if (temp_handle->event_queue != NULL) {
        QueueHandle_t temp_queue = temp_handle->event_queue;
        temp_handle->event_queue = NULL;
        vQueueDelete(temp_queue);
    }
    
    if (i2c_is_created) {
        ESP_ERROR_CHECK(i2c_master_bus_rm_device(temp_handle->lcd_i2c));
        ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));
        temp_handle->lcd_i2c = NULL;
    }

    if(temp_handle->rc522 != NULL) {
        rc522_deinit(temp_handle->rc522);
    }
    
    if(wifi_is_init) {
        ESP_ERROR_CHECK(wifi_destroy());
    }

    if (temp_handle != NULL) {
        free(temp_handle);
    }

    return 1;
}