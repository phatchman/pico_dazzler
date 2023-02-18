/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#ifndef _PARSE_DESCRIPTOR_H
#define _PARSE_DESCRIPTOR_H
#include <stdint.h>

/* 
 * Joystick definition for reading a HID Report
 * Contains the byte offsets and bitmasks used to parse a HID report
 * and extract the x/y controls and button presses
 */
struct joystick_bytes
{
    uint8_t report_id; 
    int     has_report_id;
    int     x_axis_byte;
    int     y_axis_byte;
    int     buttons_byte;
    uint8_t b1_mask;
    uint8_t b2_mask;
    uint8_t b3_mask;
    uint8_t b4_mask;
    /* TODO: bool */
    uint8_t xy_set;
    uint8_t b1_set;
    uint8_t b2_set;
    uint8_t b3_set;
    uint8_t b4_set;
};

uint8_t parse_report_descriptor(uint16_t pid, uint8_t const* desc_report, uint16_t desc_len, struct joystick_bytes *joystick_definition);

#endif