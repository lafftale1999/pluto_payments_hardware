#ifndef WIFI_IMPLEMENTATION_H
#define WIFI_IMPLEMENTATION_H

#include "esp_err.h"
#include <stdbool.h>

#define WIFI_MAX_WAIT_MS    5000

esp_err_t wifi_init();
bool wifi_is_connected();
esp_err_t wait_for_connection(void);
esp_err_t wifi_destroy();

#endif