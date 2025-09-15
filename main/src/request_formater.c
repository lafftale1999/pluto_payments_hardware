#include "request_formater.h"

#include <string.h>

#include "freeRTOS/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "mbedtls/sha256.h"

const char *SHA_TAG = "SHA256";

uint8_t create_request_body(const char* keys[], const char* values[], uint8_t key_value_len,  char* out, size_t out_len) {
    memset(out, 0, out_len);

    snprintf(out, out_len, "{");

    for(size_t i = 0; i < key_value_len; i++) {
        strncat(out, "\"", out_len - strlen(out) - 1);
        strncat(out, keys[i], out_len - strlen(out) - 1);
        strncat(out, "\":\"", out_len - strlen(out) - 1);
        strncat(out, values[i], out_len - strlen(out) - 1);
        strncat(out, "\"", out_len - strlen(out) - 1);
        if(i < key_value_len - 1) {
            strncat(out, ",", out_len - strlen(out) - 1);
        }
    }

    strncat(out, "}", out_len - strlen(out) - 1);

    return 0;
}

// create headers
// get date