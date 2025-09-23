#ifndef HTTPS_IMPLEMENTATION_H_
#define HTTPS_IMPLEMENTATION_H_

#include "esp_tls.h"

#define HTTPS_TASK_STACK_DEPTH  8192
#define HTTPS_MAX_URL_SIZE 255
#define MAX_HTTPS_REQUEST_BUFFER 1024
#define MAX_HTTPS_OUTPUT_BUFFER 1024
#define REQUEST_BODY_SIZE 512
#define MAX_PATH_LENGTH 128
#define MAX_TIMEOUT_MS  5000

typedef enum {
    GET,
    POST
} HTTPS_REQUEST_TYPE;

typedef struct {
    char url[HTTPS_MAX_URL_SIZE];
    // char path[MAX_PATH_LENGTH];
    TaskHandle_t caller;
    esp_err_t status;
    char hmac[65];
    char response_buffer[MAX_HTTPS_OUTPUT_BUFFER + 1];
    char request[MAX_HTTPS_REQUEST_BUFFER + 1];
    char request_body[REQUEST_BODY_SIZE];
    HTTPS_REQUEST_TYPE https_request_type;
} https_request_args_t;

void build_post_request(https_request_args_t *args, const char **keys, const char **values, uint8_t list_len);

#endif