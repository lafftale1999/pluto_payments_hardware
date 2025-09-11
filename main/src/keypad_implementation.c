#include "keypad_implementation.h"
#include "4x4_matrix.h"
#include "pluto_events.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <stdbool.h>

static bool task_is_running = true;

void _4x4_matrix_task(void *args) {

    QueueHandle_t queue = (QueueHandle_t)args;
    if(queue == NULL) task_is_running = false;

    char key;

    while(task_is_running) {
        key = _4x4_matrix_get_key_press();

        pluto_event_handle_t event = {
            .event_type = EV_KEY,
            .key.key_pressed = key
        };

        xQueueSend(queue, &event, portMAX_DELAY);
    }

    vTaskDelete(NULL);
}