#include "pluto_system.h"
#include "pluto_events.h"
#include "lcd_1602.h"
#include "error_checks.h"
#include "rc522_implementation.h"
#include "keypad_implementation.h"
#include "wifi_implementation.h"

#include <ctype.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_err.h"

#define PLUTO_MENU_WAIT_TIME_MS 10000
#define PLUTO_WIFI_RECONNECT_TIME_MS 60000
#define PLUTO_AMOUNT_MAX_LEN 8
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
    SYS_WIFI_RECONNECTED
}pluto_system_state;

typedef struct pluto_system {
    QueueHandle_t event_queue;
    rc522_handle_t rc522;
    pluto_system_state current_state;
    pluto_system_state last_state;
    i2c_master_dev_handle_t lcd_i2c;
}pluto_system;

static void pluto_update_state(pluto_system_handle_t handle, pluto_system_state state) {
    handle->last_state = handle->current_state;
    handle->current_state = state;
}

static void pluto_wifi_state_logic(pluto_system_handle_t handle, pluto_event_handle_t event) {
    if (!event.wifi.isConnected) {
        lcd_1602_send_string(handle->lcd_i2c, "Wifi lost...\nReconnecting...");
        ESP_ERROR_CHECK(wifi_wait_for_connection(PLUTO_WIFI_RECONNECT_TIME_MS));

        lcd_1602_send_string(handle->lcd_i2c, "Wifi reconnected");
        pluto_update_state(handle, SYS_WIFI_RECONNECTED);

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

// check payment

// make payment

// get user information

// render amount
void pluto_render_amount(pluto_system_handle_t handle, char *buf, size_t buf_size, const char *prompt, const char *amount) {
    
    memset(buf, ' ', buf_size);
    buf[buf_size - 1] = '\0';
    
    uint8_t copied_chars = 0;
    uint8_t buf_index = 0;
    
    while(*prompt != '\0' && copied_chars <= LCD_1602_SCREEN_CHAR_WIDTH) {
        buf[buf_index++] = *prompt;
        prompt++;
        copied_chars++;
    }

    size_t amount_len = strlen(amount);
    if (amount_len > LCD_1602_SCREEN_CHAR_WIDTH) amount_len = LCD_1602_SCREEN_CHAR_WIDTH;

    uint8_t amount_start = buf_size - amount_len - 1;

    for(size_t i = amount_start; i < buf_size - 1; i++) {
        buf[i] = *amount;
        amount++;
    }
    
    lcd_1602_send_string(handle->lcd_i2c, buf);
}

// create payment
static void pluto_create_payment(pluto_system_handle_t handle) {
    pluto_event_handle_t event;

    char display_string [(LCD_1602_SCREEN_CHAR_WIDTH * LCD_1602_MAX_ROWS) + 1];
    
    pluto_render_amount(handle, display_string, sizeof(display_string), "Enter Amount:", "0");
    
    char amount[PLUTO_AMOUNT_MAX_LEN] = "0";
    uint8_t chars_entered = 1;

    bool comma_entered = false;
    uint8_t decimals_entered = 0;
    uint8_t max_decimals = 2;

    while (true) {
        if (!xQueueReceive(handle->event_queue, &event, pdMS_TO_TICKS(PLUTO_MENU_WAIT_TIME_MS))) break;

        if (event.event_type == EV_KEY) {
            if (event.key.key_pressed == 'A') {
                // Get user information
                break;
            }

            else if (event.key.key_pressed == 'C') {
                break;
            }

            // Delete entered symbols
            else if (event.key.key_pressed == 'D') {
                // If there is amount entered
                if (chars_entered > 0) {
                    // if its a comma
                    if(amount[chars_entered - 1] == ',' ){
                        comma_entered = false;
                        decimals_entered = 0;
                    }
                    // if its a decimal
                    else if (comma_entered && decimals_entered > 0) decimals_entered--;
                    
                    amount[--chars_entered] = '\0';
                }
                
                // if there's no characters left
                if (chars_entered == 0) {
                    amount[0] = '0';
                    amount[1] = '\0';
                    chars_entered = 1;
                }

                pluto_render_amount(handle, display_string, sizeof(display_string), "Enter Amount:", amount);
            }
            
            else if (chars_entered < PLUTO_AMOUNT_MAX_LEN - 1 && decimals_entered < max_decimals) {
                if (isdigit((unsigned char)event.key.key_pressed)) {
                    // resetting if its the first entry
                    if (chars_entered == 1 && amount[0] == '0') chars_entered = 0;
                    amount[chars_entered++] = event.key.key_pressed;
                    amount[chars_entered] = '\0';
                    if (comma_entered) decimals_entered++;
                }
                else if (event.key.key_pressed == '*' && !comma_entered) {
                    if(chars_entered == 0) {
                        amount[chars_entered++] = '0';
                    }

                    amount[chars_entered++] = ',';
                    amount[chars_entered] = '\0';
                    comma_entered = true;
                }

                pluto_render_amount(handle, display_string, sizeof(display_string), "Enter Amount:", amount);
            }
        }

        else if (event.event_type == EV_WIFI) {
            pluto_wifi_state_logic(handle, event);
            break;
        }
    }

    lcd_1602_clear_screen(handle->lcd_i2c);
}

// wakeup
static void pluto_run_menu(pluto_system_handle_t handle) {
    pluto_event_handle_t event;
    
    pluto_update_state(handle, SYS_WAITING);
    lcd_1602_send_string(handle->lcd_i2c, "A:New payment\nC:Cancel");

    while(true) {
        if (!xQueueReceive(handle->event_queue, &event, pdMS_TO_TICKS(PLUTO_MENU_WAIT_TIME_MS))) continue;

        if (event.event_type == EV_KEY) {
            if (event.key.key_pressed == 'A') {
                pluto_create_payment(handle);
                break;
            }
            else if (event.key.key_pressed == 'C') {
                break;
            }
        }

        else if (event.event_type == EV_WIFI) {
            pluto_wifi_state_logic(handle, event);
            break;
        }
    }

    lcd_1602_clear_screen(handle->lcd_i2c);
}

uint8_t pluto_run(pluto_system_handle_t handle) {
    if (handle == NULL) {
        ESP_LOGE(PLUTO_TAG, "Handle not initialized when running");
        return 1;
    }

    pluto_event_handle_t event;

    while (true) {
        lcd_1602_clear_screen(handle->lcd_i2c);

        if (handle->current_state != SYS_SLEEPING) {
            xQueueReset(handle->event_queue);
            pluto_update_state(handle, SYS_SLEEPING);
        }

        if (!xQueueReceive(handle->event_queue, &event, portMAX_DELAY)) continue;
        
        switch (event.event_type) {
            case EV_KEY:
                pluto_run_menu(handle);
                break;
            
            case EV_WIFI:
                pluto_wifi_state_logic(handle, event);
                break;

            default:
                continue;
        }
    }

    return 0;
}

uint8_t pluto_system_init(pluto_system_handle_t *handle) {
    if (handle == NULL || *handle != NULL) {
        ESP_LOGE(PLUTO_TAG, "Handle already initialized");
        return 1;
    }

    bool i2c_is_created = false;
    bool wifi_is_init = false;

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

    const uint8_t keypad_rows[] = KEYPAD_ROW_PINS;
    const uint8_t keypad_cols[] = KEYPAD_COL_PINS;

    // INITIALIZE KEYBOARD
    if (_4x4_matrix_init(keypad_rows, keypad_cols) != 0) {
        ESP_LOGE(PLUTO_TAG, "Failed to initialize keyboard");
        goto exit;
    }

    // INITIALIZE RC522
    if (rc522_init(&temp_handle->rc522, temp_handle->event_queue) != 0) {
        ESP_LOGE(PLUTO_TAG, "Failed to initialize RC522 scanner");
        goto exit;
    }

    // INITIALIZE WIFI
    lcd_1602_send_string(temp_handle->lcd_i2c, "Connecting to\ninternet...");
    if (wifi_init() != 0) {
        ESP_LOGE(PLUTO_TAG, "Failed to initialize Wi-fi");
        goto exit;
    } else {
        wifi_is_init = true;
    }

    // WAIT FOR INTERNET CONNECTION
    if (wifi_wait_for_connection(20000) != 0) {
        ESP_LOGE(PLUTO_TAG, "Failed to connect to Wi-fi");
        goto exit;
    }

    temp_handle->current_state = SYS_SLEEPING;

    *handle = temp_handle;
    xTaskCreate(wifi_check_status, "wifi_check_status", 2048, temp_handle->event_queue, 5, NULL);
    xTaskCreate(_4x4_matrix_task, "_4x4_matrix_task", KEYPAD_TASK_WORD_SIZE, temp_handle->event_queue, 5, NULL);

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