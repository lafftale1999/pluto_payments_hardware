#ifndef PLUTO_SYSTEM_H
#define PLUTO_SYSTEM_H

#include <stdint.h>

typedef struct pluto_system* pluto_system_handle_t;

uint8_t pluto_system_init(pluto_system_handle_t *handle);
uint8_t pluto_run(pluto_system_handle_t handle);

#endif