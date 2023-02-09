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