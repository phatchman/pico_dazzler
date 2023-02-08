#ifndef _USB_JOYSTICK_H_
#define _USB_JOYSTICK_H_
/* This structure based on a USB SNES controller. */
/* Needs to be customised for other controllers */

#include <stdint.h>
#include "parse_descriptor.h"

typedef struct 
{
    uint8_t connected;
    uint8_t dev_addr;
    uint8_t instance;
    uint8_t x;
    uint8_t y;
    uint8_t buttons;
    /* TODO: bool  */
    uint8_t    b1;
    uint8_t    b2;
    uint8_t    b3;
    uint8_t    b4;
    uint8_t prev_x;
    uint8_t prev_y;
    uint8_t prev_buttons;
    struct  joystick_bytes offsets;
} usb_joystick;

/* Poll usb for input */
void schedule_joy_input();

#endif