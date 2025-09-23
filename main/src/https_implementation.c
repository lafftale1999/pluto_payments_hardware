#include "https_implementation.h"

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_sntp.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "sdkconfig.h"
#include "include/time_sync.h"
#include "credentials.h"

static const char *TAG = "HTTPS";

// Maps out where in the firmware our server certificate exists
extern const uint8_t ca_root_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t ca_root_cert_pem_end[]   asm("_binary_ca_cert_pem_end");

extern const uint8_t pluto_cert_pem_start[]     asm("_binary_client_cert_pem_start");
extern const uint8_t pluto_cert_pem_end[]       asm("_binary_client_cert_pem_end");

extern const uint8_t pluto_key_pem_start[]      asm("_binary_client_key_pem_start");
extern const uint8_t pluto_key_pem_end[]        asm("_binary_client_key_pem_end");

void https_send_request(esp_tls_cfg_t cfg, https_request_args_t *args)
{
    bool got_status_line = false;
    char buf[MAX_HTTPS_OUTPUT_BUFFER + 1];
    uint8_t request_buf_index = 0;
    int ret, len;

    esp_tls_t *tls = esp_tls_init();
    if (!tls) {
        ESP_LOGE(TAG, "Failed to allocate esp_tls handle!");
        return;
    }

    if (esp_tls_conn_http_new_sync(args->url, &cfg, tls) == 1) {
        ESP_LOGI(TAG, "Connection established...");
    } else {
        ESP_LOGE(TAG, "Connection failed...");
        int esp_tls_code = 0, esp_tls_flags = 0;
        esp_tls_error_handle_t tls_e = NULL;
        esp_tls_get_error_handle(tls, &tls_e);
        /* Try to get TLS stack level error and certificate failure flags, if any */
        ret = esp_tls_get_and_clear_last_error(tls_e, &esp_tls_code, &esp_tls_flags);
        if (ret == ESP_OK) {
            ESP_LOGE(TAG, "TLS error = -0x%x, TLS flags = -0x%x", esp_tls_code, esp_tls_flags);
        }
        goto cleanup;
    }

    size_t written_bytes = 0;
    do {
        ret = esp_tls_conn_write(tls,
                                 args->request + written_bytes,
                                 strlen(args->request) - written_bytes);
        if (ret >= 0) {
            ESP_LOGI(TAG, "%d bytes written", ret);
            written_bytes += ret;
        } else if (ret != ESP_TLS_ERR_SSL_WANT_READ  && ret != ESP_TLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "esp_tls_conn_write  returned: [0x%02X](%s)", ret, esp_err_to_name(ret));
            goto cleanup;
        }
    } while (written_bytes < strlen(args->request));

    memset(args->response_buffer, 0, MAX_HTTPS_OUTPUT_BUFFER);

    do {
        len = sizeof(buf) - 1;
        memset(buf, 0x00, sizeof(buf));
        ret = esp_tls_conn_read(tls, (char *)buf, len);

        if (ret == ESP_TLS_ERR_SSL_WANT_WRITE || ret == ESP_TLS_ERR_SSL_WANT_READ) {
            continue;
        } else if (ret < 0) {
            ESP_LOGE(TAG, "esp_tls_conn_read returned [-0x%02X](%s)", -ret, esp_err_to_name(ret));
            break;
        } else if (ret == 0) {
            ESP_LOGI(TAG, "connection closed");
            break;
        }

        buf[ret] = '\0';

        if (!got_status_line) {
            got_status_line = true;
            args->status = ESP_FAIL;

            char *status_line = strtok((char *)buf, "\r\n");
            if (status_line && strncmp(status_line, "HTTP/", 5) == 0) {
                int status_code = atoi(status_line + 9);

                args->status = (status_code == 200) ? ESP_OK : ESP_FAIL;
            }
        }

        if(request_buf_index + ret < MAX_HTTPS_OUTPUT_BUFFER - 1){
            for(int i = 0; i < ret; i++) {
                args->response_buffer[request_buf_index++] = buf[i];
            }
    
            args->response_buffer[request_buf_index++] = '\n';
        } else {
            ESP_LOGE(TAG, "Response buffer overflow");
            break;
        }
    } while (1);

    args->response_buffer[request_buf_index++] = '\0';

cleanup:
    esp_tls_conn_destroy(tls);
}

void https_send_with_cert(https_request_args_t *args)
{
    esp_tls_cfg_t cfg = {
        .cacert_buf = (const unsigned char *) ca_root_cert_pem_start,
        .cacert_bytes = ca_root_cert_pem_end - ca_root_cert_pem_start,

        .clientcert_buf = (const unsigned char*) pluto_cert_pem_start,
        .clientcert_bytes = pluto_cert_pem_end - pluto_cert_pem_start,

        .clientkey_buf = (const unsigned char*) pluto_key_pem_start,
        .clientkey_bytes = pluto_key_pem_end - pluto_key_pem_start
    };

    https_send_request(cfg, args);
}

void https_post_task(void *pvparameters)
{
    https_request_args_t *args = (https_request_args_t*) pvparameters;
    
    https_send_with_cert(args);

    if(args->caller != NULL) {
        xTaskNotifyGive(args->caller);
    }
    vTaskDelete(NULL);
}

void set_request_url(https_request_args_t *args) {
    memset(args->url, 0, HTTPS_MAX_URL_SIZE);
    snprintf(args->url, HTTPS_MAX_URL_SIZE, "https://%s%s", SERVER_HOST, PLUTO_PAYMENT_API);
}

esp_err_t build_request(https_request_args_t *args) {
    memset(args->request, 0, MAX_HTTPS_REQUEST_BUFFER +1);
    
    char content_length[64] = "";
    snprintf(content_length, sizeof(content_length),
            "Content-Length: %d", (int)strlen(args->request_body));

    int written = snprintf(args->request, MAX_HTTPS_REQUEST_BUFFER + 1,
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Authorization: %s\r\n"
        "User-Agent: esp-idf/1.0 esp32\r\n"
        "Connection: close\r\n"
        "Content-Type: application/json\r\n"
        "%s\r\n"
        "\r\n"
        "%s",
        PLUTO_PAYMENT_API, 
        SERVER_HOST,
        args->hmac,
        content_length,
        args->request_body);
    
    if (written < 0 || written >= MAX_HTTPS_REQUEST_BUFFER + 1) {
        ESP_LOGE(TAG, "Unable to create request");
        return ESP_FAIL;
    }

    return ESP_OK;
}

// Solution found online for extracting body from http response
static char* extract_body(char *resp) {
    char *p = strstr(resp, "\r\n\r\n");
    if (!p) p = strstr(resp, "\n\n");
    return p ? p + ((p[1] == '\n' && p[-1] == '\r') ? 4 : 2) : resp;
}

esp_err_t https_create_and_send_request(char *request_body, char *hmac, char *out, size_t out_size) {
    https_request_args_t *args = (https_request_args_t*)calloc(1, sizeof(https_request_args_t));

    if (args == NULL) {
        ESP_LOGE(TAG, "Unable to heap allocate args");
        return ESP_FAIL;
    }

    snprintf(args->hmac, sizeof(args->hmac), "%s", hmac);
    snprintf(args->request_body, sizeof(args->request_body), "%s", request_body);
    args->caller = xTaskGetCurrentTaskHandle();
    args->status = ESP_FAIL;
    args->https_request_type = POST;

    set_request_url(args);
    build_request(args);

    xTaskCreate(https_post_task, "https_post_task", HTTPS_TASK_STACK_DEPTH, args, 5, NULL);

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    esp_err_t post_status = args->status;

    char *temp_body = extract_body(args->response_buffer);
    char body[HTTPS_MAX_RESPONSE_BODY_LCD] = {0};
    size_t body_index = 0;

    // TRIM STARTING CRLF
    while (*temp_body == '\r' || *temp_body == '\n') temp_body++;

    while (*temp_body && isascii((unsigned char)*temp_body) && body_index < sizeof(body) - 1) {
        unsigned char c = (unsigned char)*temp_body++;

        // REMOVE CR
        if (c == '\r') continue;

        body[body_index++] = (char)c;
    }

    // TRIM CRLF
    while (body_index && (body[body_index-1] == '\n' || body[body_index-1] == '\r')) {
        body_index--;
    }
    body[body_index] = '\0';

    if (strlen(body) > 32) {
        snprintf(out, out_size, "Unkown error");
    } else {
        snprintf(out, out_size, "%s", body);
    }

    free(args);

    return post_status;
}