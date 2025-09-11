#ifndef KEYPAD_IMPLEMENTATION_H
#define KEYPAD_IMPLEMENTATION_H

#define KEYPAD_TASK_WORD_SIZE   1024

#include "4x4_matrix.h"
/**
    Starts up a task to add pluto_event_handle_t to the passed QueueHandle_t
    @param QueueHandle_t queue - queue to add events to.
*/
void _4x4_matrix_task(void *args);

#endif