#ifndef SYSTEM_FORMATTER_H_
#define SYSTEM_FORMATTER_H_

#include <stdint.h>
#include <stdio.h>
#include "esp_err.h"

#define SHA256_DIGEST_SIZE 32
#define SHA256_OUT_SIZE     (SHA256_DIGEST_SIZE * 2) + 1

uint8_t create_request_body(const char* keys[], const char* values[], uint8_t key_value_len,  char* out, size_t out_len);
esp_err_t hash_sha256(const unsigned char *input_buffer, size_t input_buffer_len, char hex_output_buffer[(SHA256_DIGEST_SIZE * 2) + 1]);

#endif