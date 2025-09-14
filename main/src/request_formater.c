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

esp_err_t hash_sha256(const unsigned char *input_buffer, size_t input_buffer_len, char hex_output_buffer[(SHA256_DIGEST_SIZE * 2) + 1]) {
    if(!input_buffer || !hex_output_buffer) {
        ESP_LOGE(SHA_TAG, "Invalid argument - Input or output buffer NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    unsigned char hash_digest[SHA256_DIGEST_SIZE];
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    esp_err_t ret = ESP_OK;

    if (mbedtls_sha256_starts(&context, 0) != 0 ||
        mbedtls_sha256_update(&context, input_buffer, input_buffer_len) != 0 ||
        mbedtls_sha256_finish(&context, hash_digest) != 0)
        {
        ESP_LOGE(SHA_TAG, "SHA256 calculation failed");
        ret = ESP_FAIL;
        goto exit;
    }
    
    for (int i = 0; i < SHA256_DIGEST_SIZE; i++) {
        sprintf(hex_output_buffer + (i * 2), "%02x", hash_digest[i]);
    }

    hex_output_buffer[SHA256_DIGEST_SIZE * 2] = '\0';

    exit:
        mbedtls_sha256_free(&context);
        return ret;
}

// create headers
// create nonce
// get date