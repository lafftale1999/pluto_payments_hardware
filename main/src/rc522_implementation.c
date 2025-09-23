#include <esp_log.h>
#include "rc522.h"
#include "driver/rc522_spi.h"
#include "rc522_picc.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "project_config.h"

#include <string.h>
#include <ctype.h>

#include "pluto_events.h"

static const char *TAG = "rc522";

static QueueHandle_t queue;
static bool rc522_is_created = false;

static bool rfid_check_check_card_format(char *card_number, size_t str_size) {
    card_number[str_size - 1] = '\0';

    while(*card_number != '\0') {
        if (isdigit((unsigned char)*card_number) ||
            isascii((unsigned char)*card_number) ||
            *card_number == ' ') {
                card_number++;
                continue;
            } else {
                return false;
            }
    }

    return true;
}

static void on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
    rc522_picc_t *picc = event->picc;

    if (picc->state == RC522_PICC_STATE_ACTIVE) {
        pluto_event_handle_t event = {
            .event_type = EV_RFID,
        };

        rc522_picc_uid_to_str(&picc->uid, event.rfid.cardNumber, sizeof(event.rfid.cardNumber));
        
        ESP_LOGI("RC522", "%s", event.rfid.cardNumber);
        
        if(!rfid_check_check_card_format(event.rfid.cardNumber, sizeof(event.rfid.cardNumber))) {
            event.event_type = EV_SCAN_FAILED;
        }

        xQueueSend(queue, &event, portMAX_DELAY);
    }
    else if (picc->state == RC522_PICC_STATE_IDLE && event->old_state >= RC522_PICC_STATE_ACTIVE) {
        ESP_LOGI(TAG, "Card has been removed");
    }
}

/**
 * Configures and installs the driver, creates the SPI communication between devices,
 * creates and registers a task to react on state changes on the PICC.
 * @param rc522_handle_t double pointer to the datastructure of rc522. If successful will point to the datastructure.
 * @param QueueHandle_t pointer to the queue for events.
 * 
 * @return 0 for success or 1 for failed.
 */
uint8_t rc522_init(rc522_handle_t *out, QueueHandle_t owner_queue)
{
    if(!out) return 1;

    if(!rc522_is_created) {
        rc522_spi_config_t spi_config = {
            .host_id = SPI3_HOST,
            .bus_config = &(spi_bus_config_t){
                .miso_io_num = RC522_SPI_BUS_GPIO_MISO,
                .mosi_io_num = RC522_SPI_BUS_GPIO_MOSI,
                .sclk_io_num = RC522_SPI_BUS_GPIO_SCLK,
            },
            .dev_config = {
                .spics_io_num = RC522_SPI_SCANNER_GPIO_SDA,
            },
            .rst_io_num = RC522_SCANNER_GPIO_RST,
        };

        rc522_driver_handle_t driver;
        ESP_ERROR_CHECK(rc522_spi_create(&spi_config, &driver));
        ESP_ERROR_CHECK(rc522_driver_install(driver));

        rc522_config_t scanner_config = {
            .driver = driver,
        };

        rc522_handle_t scanner = NULL;

        ESP_ERROR_CHECK(rc522_create(&scanner_config, &scanner));
        ESP_ERROR_CHECK(rc522_register_events(scanner, RC522_EVENT_PICC_STATE_CHANGED, on_picc_state_changed, NULL));

        rc522_is_created = true;

        *out = scanner;
    }

    if(!queue && owner_queue != NULL) {
        queue = owner_queue;
    }
    
    return 0;
}

uint8_t rc522_deinit(rc522_handle_t handle) {
    if(handle != NULL) {
        rc522_destroy(handle);
    }
    
    if(queue != NULL) {
        queue = NULL;
    }
    
    return 0;
}