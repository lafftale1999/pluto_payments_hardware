#include "security_measures.h"

#include "esp_random.h"
#include "freeRTOS/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "mbedtls/sha256.h"

#include <string.h>

void sec_generate_nonce(char *out_buf, size_t buf_len) {
    uint32_t random_number = esp_random();
    const unsigned char temp_buf[11];
    snprintf(temp_buf, sizeof(temp_buf), "%d", random_number);
    
    hash_sha256(temp_buf, sizeof(temp_buf), out_buf);
}

esp_err_t hash_sha256(const unsigned char *input_buffer, size_t input_buffer_len, char hex_output_buffer[SHA256_OUT_BUF_SIZE]) {
    if(!input_buffer || !hex_output_buffer) {
        ESP_LOGE(TAG, "Invalid argument - Input or output buffer NULL");
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
        ESP_LOGE(TAG, "SHA256 calculation failed");
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