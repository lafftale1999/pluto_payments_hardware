#ifndef _RC522_IMPLEMENTATION_H_
#define _RC522_IMPLEMENTATION_H_

#include <stdint.h>

#include "esp_err.h"
#include "driver/gpio.h"

#include "rc522.h"

/**
 * Configures and installs the driver, creates the SPI communication between devices,
 * creates and registers a task to react on state changes on the PICC.
 * @param rc522_handle_t double pointer to the datastructure of rc522. If successful will point to the datastructure.
 * @param QueueHandle_t pointer to the queue for events.
 * 
 * @return 0 for success or 1 for failed.
 */
uint8_t rc522_init(rc522_handle_t *out, QueueHandle_t owner_queue);

/**
 * Destroys the rc522 handle and points the QueueHandle_t back to NULL.
 * @param rc522_handle_t pointer to structure that will be destroyed.
 * 
 * @return 0 for success.
 */
uint8_t rc522_deinit(rc522_handle_t handle);

#endif