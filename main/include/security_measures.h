#ifndef SECURITY_MEASURES_H_
#define SECURITY_MEASURES_H_

#include <ctype.h>
#include "esp_err.h"

#define SHA256_DIGEST_SIZE 32
#define SHA256_OUT_BUF_SIZE ((SHA256_DIGEST_SIZE * 2) + 1)
#define CANONICAL_STRING_SIZE 256

void sec_generate_nonce(char *out_buf, size_t buf_len);
esp_err_t hash_sha256(const unsigned char *input_buffer, size_t input_buffer_len, char hex_output_buffer[SHA256_OUT_BUF_SIZE]);
void build_canonical_string(const char *hashed_body, char *out_buf, size_t out_buf_size);
#endif