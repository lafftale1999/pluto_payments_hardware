#ifndef PLUTO_SYSTEM_H
#define PLUTO_SYSTEM_H

#include <stdint.h>

#define KEYPAD_ROW_PINS {GPIO_NUM_26, GPIO_NUM_25, GPIO_NUM_17, GPIO_NUM_16}
#define KEYPAD_COL_PINS {GPIO_NUM_27, GPIO_NUM_14, GPIO_NUM_12, GPIO_NUM_13}

typedef struct pluto_system* pluto_system_handle_t;

uint8_t pluto_system_init(pluto_system_handle_t *handle);
uint8_t pluto_run(pluto_system_handle_t handle);

#endif