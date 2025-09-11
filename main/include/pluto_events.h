#ifndef PLUTO_APP_EVENTS_H
#define PLUTO_APP_EVENTS_H

typedef enum pluto_payment_event {
    EV_RFID,
    EV_KEY,
    EV_PIN
}pluto_payment_event;

typedef struct pluto_event_handle_t {
    pluto_payment_event event_type;

    union {
        struct {char cardNumber[20];}rfid;
        struct {char code[5];}pin;
        struct {char key_pressed;}key;
    };
}pluto_event_handle_t;

#endif