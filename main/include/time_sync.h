#ifndef PLUTO_TIME_SYNC_H
#define PLUTO_TIME_SYNC_H

#include "esp_err.h"

#define TIME_STRING_SIZE    64

void time_set_timezone();
void time_get_current_time(char *buf, size_t buf_size);
esp_err_t time_update_and_store_in_nvs(void *args);
esp_err_t time_updated_from_nvs();

#endif