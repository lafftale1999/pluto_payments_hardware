#ifndef LCD_RENDER_H_
#define LCD_RENDER_H_

#include "driver/i2c_master.h"

void lcd_render_amount (
    i2c_master_dev_handle_t handle,
    char *buf, size_t buf_size,
    const char *prompt,
    const char *amount,
    const char *CURRENCY
);

void lcd_render_pin (
    i2c_master_dev_handle_t handle,
    char *lcd_buffer,
    size_t lcd_buffer_size,
    const char *header,
    const char *prompt,
    uint8_t entered_pin_length,
    size_t max_pin_len
);

#endif