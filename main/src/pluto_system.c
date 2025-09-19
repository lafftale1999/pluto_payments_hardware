#include "pluto_system.h"
#include "pluto_events.h"
#include "lcd_1602.h"
#include "error_checks.h"
#include "rc522_implementation.h"
#include "keypad_implementation.h"
#include "wifi_implementation.h"
#include "time_sync.h"
#include "security_measures.h"
#include "request_formater.h"
#include "http_implementation.h"
#include "lcd_render.h"
#include "credentials.h"

#include <ctype.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_mac.h"

#define PLUTO_ERROR_MESSAGE_TIME_MS 2500
#define PLUTO_MENU_WAIT_TIME_MS 20000
#define PLUTO_WIFI_RECONNECT_TIME_MS 60000
#define PLUTO_AMOUNT_MAX_LEN 8
#define PLUTO_CARD_LENGTH 20
#define PLUTO_PIN_LENGTH 5
#define PLUTO_HTTP_HEADER_SIZE 100
#define MAC_ADDRESS_LEN 18

const char *PLUTO_TAG = "PLUTO_SYSTEM";
const char CURRENCY[] = "SEK";

// STATE ENUM
typedef enum pluto_system_state {
    SYS_SLEEPING,
    SYS_WAITING,
    SYS_CREATE_PAYMENT,
    SYS_MAKE_PAYMENT,
    SYS_CHECK_PAYMENT,
    SYS_WIFI_RECONNECTED
} pluto_system_state;

// PAYMENT STRUCT
typedef struct pluto_payment {
    char amount[PLUTO_AMOUNT_MAX_LEN];
    char card_number[PLUTO_CARD_LENGTH];
    char pin_code[SHA256_OUT_BUF_SIZE];
    char currency[sizeof(CURRENCY)];
    char date[TIME_STRING_SIZE];
    char nonce[SHA256_OUT_BUF_SIZE];
    char operation[20];
    char device_id[MAC_ADDRESS_LEN];
} pluto_payment;

// JSON KEY ENUM
typedef enum pluto_payment_keys_t {
    PAYMENT_AMOUNT,
    PAYMENT_CARD_NUMBER,
    PAYMENT_PIN_CODE,
    PAYMENT_CURRENCY,
    PAYMENT_DATE,
    PAYMENT_NONCE,
    PAYMENT_OPERATION,
    PAYMENT_DEVICE_ID,
    PAYMENT_KEY_SIZE
} pluto_payment_keys_t;

// JSON KEYS
static char *payment_keys[] = {"amount", "cardNumber", "pinCode", "currency", "timeStamp", "nonce", "operation", "deviceMacAddress"};

// HTTP HEADERS
typedef enum pluto_payment_http_headers {
    HTTP_HEADER_CONTENT_TYPE,
    HTTP_HEADER_AUTHORIZATION,
    HTTP_HEADER_SIZE
} pluto_payment_http_headers;

#define PAYMENT_HTTP_HEADERS {"Content-Type", "Authorization"}
#define PAYMENT_HTTP_VALUES  {"application/json"}

// DEFINITION OF PLUTO HANDLE
typedef struct pluto_system {
    QueueHandle_t event_queue;
    rc522_handle_t rc522;
    pluto_system_state current_state;
    pluto_system_state last_state;
    i2c_master_dev_handle_t lcd_i2c;
} pluto_system;

static bool send_request(pluto_system_handle_t handle, char *hmac_hashed, char *request_body) {
    http_request_args_t *args = calloc(1, sizeof(*args));
    args->caller = xTaskGetCurrentTaskHandle();
    args->status = ESP_FAIL;

    snprintf(args->hmac, sizeof(args->hmac), "%s", hmac_hashed);
    snprintf(args->post_data, sizeof(args->post_data), "%s", request_body);
    snprintf(args->url, sizeof(args->url), "%s%s", PLUTO_URL, PLUTO_PAYMENT_API);
    
    ESP_LOGI(PLUTO_TAG, "%s", args->url);

    xTaskCreate(http_post_task, "http_post", HTTP_POST_TASK_STACK_SIZE, args, 5, NULL);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    bool payment_accepted = args->status == ESP_OK;

    if (strlen(args->response_buffer) > 32) {
        lcd_1602_send_string(handle->lcd_i2c, "Unkown Error\nTry again...");
    } else {
        lcd_1602_send_string(handle->lcd_i2c, args->response_buffer);
    }

    free(args);

    return payment_accepted;
}

static void pluto_create_values(pluto_payment *payment, char *out_buf[]) {
    out_buf[PAYMENT_AMOUNT] = payment->amount;
    out_buf[PAYMENT_CARD_NUMBER] = payment->card_number;
    out_buf[PAYMENT_PIN_CODE] = payment->pin_code;
    out_buf[PAYMENT_CURRENCY] = payment->currency;
    out_buf[PAYMENT_DATE] = payment->date;
    out_buf[PAYMENT_NONCE] = payment->nonce;
    out_buf[PAYMENT_OPERATION] = payment->operation;
    out_buf[PAYMENT_DEVICE_ID] = payment->device_id;
}

static void pluto_update_state(pluto_system_handle_t handle, pluto_system_state state) {
    handle->last_state = handle->current_state;
    handle->current_state = state;
}

static void pluto_wifi_state_logic(pluto_system_handle_t handle, pluto_event_handle_t event) {
    if (!event.wifi.isConnected) {
        ESP_LOGE(PLUTO_TAG, "WIFI DISCONNECTED");
        lcd_1602_send_string(handle->lcd_i2c, "Wifi lost...\nReconnecting...");
        ESP_ERROR_CHECK(wifi_wait_for_connection(PLUTO_WIFI_RECONNECT_TIME_MS));

        lcd_1602_send_string(handle->lcd_i2c, "Wifi reconnected");
        pluto_update_state(handle, SYS_WIFI_RECONNECTED);

        vTaskDelay(pdMS_TO_TICKS(PLUTO_ERROR_MESSAGE_TIME_MS));
    }
}

static void get_mac_address(pluto_payment *payment) {
    unsigned char mac[6] = {0};
    esp_efuse_mac_get_default(mac);
    
    snprintf(payment->device_id, sizeof(payment->device_id), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);

    ESP_LOGI(PLUTO_TAG, "%s", payment->device_id);
}

static bool pluto_get_pin_code(pluto_system_handle_t handle, pluto_payment *payment) {
    bool pin_code_entered = false;
    pluto_event_handle_t event;
    char buf[LCD_1602_SCREEN_CHAR_WIDTH * LCD_1602_MAX_ROWS + 1];
    
    char header[LCD_1602_SCREEN_CHAR_WIDTH + 1];
    snprintf(header, sizeof(header), "%s %s", payment->amount, CURRENCY);
    char prompt[] = "Pin: ";
    
    char pin_code[PLUTO_PIN_LENGTH];

    uint8_t pin_code_len = 0;
    lcd_render_pin(handle->lcd_i2c, buf, sizeof(buf), header, prompt, pin_code_len, PLUTO_PIN_LENGTH);

    while(true) {
        if (!xQueueReceive(handle->event_queue, &event, pdMS_TO_TICKS(PLUTO_MENU_WAIT_TIME_MS))) {
            lcd_1602_send_string(handle->lcd_i2c, "Payment failed");
            vTaskDelay(pdMS_TO_TICKS(PLUTO_ERROR_MESSAGE_TIME_MS));
            break;
        }

        if (event.event_type == EV_KEY) {
            if (isdigit((unsigned char)event.key.key_pressed)) {
                if (pin_code_len < PLUTO_PIN_LENGTH - 1) {
                    pin_code[pin_code_len++] = event.key.key_pressed;
                }
                if (pin_code_len == PLUTO_PIN_LENGTH - 1) {
                    pin_code[pin_code_len] = '\0';
                    pin_code_entered = true;
                }
            }
            else if (event.key.key_pressed == 'A' ) {
                if (pin_code_entered) {
                    hash_sha256((const unsigned char*)pin_code, strlen(pin_code), payment->pin_code);
                    break;
                } 
                else {
                    lcd_1602_send_string(handle->lcd_i2c, "Enter 4 digits");
                    vTaskDelay(pdMS_TO_TICKS(PLUTO_ERROR_MESSAGE_TIME_MS));
                    xQueueReset(handle->event_queue);
                }
            }
            else if (event.key.key_pressed == 'D' && pin_code_len > 0) {
                pin_code[--pin_code_len] = '\0';
            }
            else if (event.key.key_pressed == 'C') {
                lcd_1602_send_string(handle->lcd_i2c, "Payment canceled");
                vTaskDelay(pdMS_TO_TICKS(PLUTO_ERROR_MESSAGE_TIME_MS));
                break;
            }
            lcd_render_pin(handle->lcd_i2c, buf, sizeof(buf), header, prompt, pin_code_len, PLUTO_PIN_LENGTH);
        }
    }
    
    return pin_code_entered;
}

static bool pluto_get_card_number(pluto_system_handle_t handle, pluto_payment *payment) {
    bool card_scanned = false;
    pluto_event_handle_t event;

    lcd_1602_send_string(handle->lcd_i2c, "Scan card...");

    rc522_start(handle->rc522);

    while (true) {
        if (!xQueueReceive(handle->event_queue, &event, pdMS_TO_TICKS(PLUTO_MENU_WAIT_TIME_MS))) {
            lcd_1602_send_string(handle->lcd_i2c, "Payment failed");
            break;
        }

        if (event.event_type == EV_RFID) {
            lcd_1602_send_string(handle->lcd_i2c, "Card scanned...");
            snprintf(payment->card_number, sizeof(payment->card_number), event.rfid.cardNumber);
            card_scanned = true;
            break;
        }
        else if (event.event_type == EV_KEY) {
            if (event.key.key_pressed == 'C') {
                lcd_1602_send_string(handle->lcd_i2c, "Payment canceled");
                break;
            }
        }
    }
    vTaskDelay(pdMS_TO_TICKS(PLUTO_ERROR_MESSAGE_TIME_MS));
    rc522_pause(handle->rc522);
    
    return card_scanned;
}

static bool pluto_get_amount(pluto_system_handle_t handle, pluto_payment *payment) {
    ESP_LOGI(PLUTO_TAG, "Entered get amount");

    pluto_event_handle_t event;
    
    char display_string [(LCD_1602_SCREEN_CHAR_WIDTH * LCD_1602_MAX_ROWS) + 1];
    
    lcd_render_amount(handle->lcd_i2c, display_string, sizeof(display_string), "Enter Amount:", "0", CURRENCY);
    
    char amount[PLUTO_AMOUNT_MAX_LEN] = "0";
    uint8_t chars_entered = 1;

    bool comma_entered = false;
    uint8_t decimals_entered = 0;
    uint8_t max_decimals = 2;

    while (true) {
        
        if (!xQueueReceive(handle->event_queue, &event, pdMS_TO_TICKS(PLUTO_MENU_WAIT_TIME_MS))) break;

        if (event.event_type == EV_KEY) {
            if (event.key.key_pressed == 'A') {
                snprintf(payment->amount, sizeof(payment->amount), "%s", amount);
                snprintf(payment->currency, sizeof(payment->currency), "%s", CURRENCY);
                return true;
            }

            else if (event.key.key_pressed == 'C') {
                break;
            }

            // Delete entered symbols
            else if (event.key.key_pressed == 'D') {
                // If there is amount entered
                if (chars_entered > 0) {
                    // if its a comma
                    if(amount[chars_entered - 1] == '.' ){
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

                lcd_render_amount(handle->lcd_i2c, display_string, sizeof(display_string), "Enter Amount:", amount, CURRENCY);
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

                    amount[chars_entered++] = '.';
                    amount[chars_entered] = '\0';
                    comma_entered = true;
                }

                lcd_render_amount(handle->lcd_i2c, display_string, sizeof(display_string), "Enter Amount:", amount, CURRENCY);
            }
        }

        else if (event.event_type == EV_WIFI) {
            ESP_LOGI(PLUTO_TAG, "Wifi event received");
            pluto_wifi_state_logic(handle, event);
            break;
        }

        ESP_LOGI(PLUTO_TAG, "Bottom of loop");
    }

    return false;
}

// create payment
static void pluto_create_payment(pluto_system_handle_t handle) {

    pluto_payment payment = {
        .operation = "send_payment"
    };

    ESP_LOGI(PLUTO_TAG, "Entered create payment");

    if (pluto_get_amount(handle, &payment) &&
        pluto_get_card_number(handle, &payment) &&
        pluto_get_pin_code(handle, &payment))
        {
        
        lcd_1602_send_string(handle->lcd_i2c, "Verifying ...");

        // get important values
        time_get_current_time(payment.date, sizeof(payment.date));
        sec_generate_nonce(payment.nonce, sizeof(payment.nonce));
        get_mac_address(&payment);
        
        // create request body
        char *payment_values[PAYMENT_KEY_SIZE] = {0};
        char request_body[HTTP_REQUEST_BODY_SIZE] = {0};
        pluto_create_values(&payment, payment_values);
        create_request_body((const char **)payment_keys, (const char **)payment_values, PAYMENT_KEY_SIZE, request_body, sizeof(request_body));

        ESP_LOGI(PLUTO_TAG, "%s", request_body);
        
        // hash body
        char hashed_body[SHA256_OUT_BUF_SIZE] = {0};
        hash_sha256((const unsigned char*) request_body, strlen(request_body), hashed_body);
        ESP_LOGI(PLUTO_TAG, "Hashed Body: %s", hashed_body);

        // create HMAC
        char device_key[] = DEVICE_KEY;
        char canonical_string[CANONICAL_STRING_SIZE] = {0};
        char hmac_hashed[SHA256_OUT_BUF_SIZE] = {0};
        build_canonical_string(hashed_body, canonical_string, sizeof(canonical_string));

        ESP_LOGI(PLUTO_TAG, "CANONICAL STRING\n%s", canonical_string);

        strncat(canonical_string, device_key, sizeof(canonical_string) - strlen(canonical_string));
        hash_sha256((const unsigned char*) canonical_string, strlen(canonical_string), hmac_hashed);
        
        ESP_LOGI(PLUTO_TAG, "HMAC: %s", hmac_hashed);
        
        send_request(handle, hmac_hashed, request_body);

        vTaskDelay(pdMS_TO_TICKS(PLUTO_ERROR_MESSAGE_TIME_MS));
    }

    lcd_1602_clear_screen(handle->lcd_i2c);
}

// wakeup
static void pluto_run_menu(pluto_system_handle_t handle) {
    pluto_event_handle_t event;
    
    pluto_update_state(handle, SYS_WAITING);
    lcd_1602_send_string(handle->lcd_i2c, "A:New payment\nC:Cancel");

    while(true) {
        if (!xQueueReceive(handle->event_queue, &event, pdMS_TO_TICKS(PLUTO_MENU_WAIT_TIME_MS))) break;

        if (event.event_type == EV_KEY) {
            if (event.key.key_pressed == 'A') {
                pluto_create_payment(handle);
                break;
            }
            else if (event.key.key_pressed == 'C') {
                break;
            }
            else if (event.key.key_pressed == '#') {
                char time[TIME_STRING_SIZE];
                time_get_current_time(time, sizeof(time));
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
    lcd_1602_send_string(temp_handle->lcd_i2c, "Connected!");

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