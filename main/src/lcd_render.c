#include "lcd_render.h"
#include "lcd_1602.h"

#include <string.h>

void lcd_render_amount (
    i2c_master_dev_handle_t handle,
    char *buf, size_t buf_size,
    const char *prompt,
    const char *amount,
    const char *CURRENCY
) {
    memset(buf, ' ', buf_size);
    buf[buf_size - 1] = '\0';
    
    uint8_t copied_chars = 0;
    uint8_t buf_index = 0;
    
    while(*prompt != '\0' && copied_chars <= LCD_1602_SCREEN_CHAR_WIDTH) {
        buf[buf_index++] = *prompt;
        prompt++;
        copied_chars++;
    }

    size_t currency_len = strlen(CURRENCY);
    size_t amount_len = strlen(amount);
    if (amount_len >  (LCD_1602_SCREEN_CHAR_WIDTH - currency_len)) amount_len = LCD_1602_SCREEN_CHAR_WIDTH - currency_len;
    uint8_t amount_start = buf_size - amount_len - currency_len - 2;

    snprintf(&buf[amount_start], currency_len + amount_len + 2, "%s %s", amount, CURRENCY);
    
    lcd_1602_send_string(handle, buf);
}

void lcd_render_pin (
    i2c_master_dev_handle_t handle,
    char *lcd_buffer,
    size_t lcd_buffer_size,
    const char *header,
    const char *prompt,
    uint8_t entered_pin_length,
    size_t max_pin_len
) {
    if (lcd_buffer_size < (LCD_1602_SCREEN_CHAR_WIDTH * LCD_1602_MAX_ROWS + 1)) {
        return;
    }

    memset(lcd_buffer, ' ', lcd_buffer_size);
    lcd_buffer[lcd_buffer_size - 1] = '\0';

    size_t header_length = strlen(header);
    if (header_length > LCD_1602_SCREEN_CHAR_WIDTH) {
        header_length = LCD_1602_SCREEN_CHAR_WIDTH;
    }
    memcpy(&lcd_buffer[0], header, header_length);

    size_t prompt_length = strlen(prompt);
    if (prompt_length + max_pin_len - 1 > LCD_1602_SCREEN_CHAR_WIDTH) {
        prompt_length = LCD_1602_SCREEN_CHAR_WIDTH - max_pin_len - 1;
    }
    memcpy(&lcd_buffer[LCD_1602_SCREEN_CHAR_WIDTH], prompt, prompt_length);

    for (size_t i = 0; i < entered_pin_length; i++) {
        lcd_buffer[LCD_1602_SCREEN_CHAR_WIDTH + prompt_length + i] = '*';
    }

    lcd_1602_send_string(handle, lcd_buffer);
}