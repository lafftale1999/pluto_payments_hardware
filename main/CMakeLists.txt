file(GLOB_RECURSE SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

idf_component_register(
    SRCS 
        "main.c"
        ${SRC_FILES}
    INCLUDE_DIRS 
        "."

    REQUIRES
        rc522
        4x4_matrix
        lcd_1602_i2c_driver
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        mbedtls
        esp_http_client
)