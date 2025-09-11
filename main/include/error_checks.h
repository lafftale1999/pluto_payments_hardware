#ifndef ERROR_CHECKS_H_
#define ERROR_CHECKS_H_

#include "esp_err.h"
#include "esp_log.h"

#define ESP_RETURN_ON_ERROR(err, caller, msg)  do {int __err_rc = (err); if (__err_rc != 0) ESP_LOGE(caller, "%s", msg) return __err_rc;} while (0)

#endif