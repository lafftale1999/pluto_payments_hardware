#ifndef PLUTO_APP_EVENTS_H
#define PLUTO_APP_EVENTS_H

#include <stdbool.h>

typedef enum pluto_event_type {
    EV_RFID,
    EV_KEY,
    EV_PIN,
    EV_WIFI,
    EV_SCAN_FAILED
}pluto_event_type;

typedef struct pluto_event_handle_t {
    pluto_event_type event_type;

    union {
        struct {char cardNumber[30];}rfid;
        struct {char code[5];}pin;
        struct {char key_pressed;}key;
        struct {bool isConnected;}wifi;
    };
}pluto_event_handle_t;

#endif