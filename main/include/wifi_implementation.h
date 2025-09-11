#ifndef WIFI_IMPLEMENTATION_H
#define WIFI_IMPLEMENTATION_H

#include "esp_err.h"
#include <stdbool.h>

#define WIFI_MAX_WAIT_MS    5000

esp_err_t wifi_init();
bool wifi_is_connected();
esp_err_t wifi_wait_for_connection(int wait_time_ms);
esp_err_t wifi_destroy();
void wifi_check_status (void *args);

#endif