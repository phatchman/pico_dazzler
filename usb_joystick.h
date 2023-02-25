/*
 * MIT License
 *
 * Copyright (c) 2023 Paul Hatchman
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef _USB_JOYSTICK_H_
#define _USB_JOYSTICK_H_
/* This structure based on a USB SNES controller. */
/* Needs to be customised for other controllers */

#include <stdint.h>
#include "parse_descriptor.h"

/*
 * The current joystick / controller state with current and previous values
 */
typedef struct 
{
    uint8_t connected;
    uint8_t dev_addr;
    uint8_t instance;
    uint8_t x;
    uint8_t y;
    uint8_t buttons;
    uint8_t b1;
    uint8_t b2;
    uint8_t b3;
    uint8_t b4;
    uint8_t prev_x;
    uint8_t prev_y;
    uint8_t prev_buttons;
    uint8_t zero_centered;              /* True if centre value of joystick is 0 e.g. XBOX controller*/
    uint8_t dead_zone;                  /* controllers don't report 0 when "centered" can cause issues in some gsames */
    struct  joystick_bytes offsets;
} usb_joystick;

/* Tiny USB Callbacks */
void joy_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len);
void joy_hid_unmount_cb(uint8_t dev_addr, uint8_t instance);
void joy_process_hid_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

/* Poll usb for input */
void joy_schedule_hid_input(void);

bool is_xbox_controller(uint16_t pid);

#endif