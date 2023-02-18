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
#include "parse_descriptor.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define TU_ATTR_PACKED __attribute__((packed))
#include "local_hid.h"


#define DEBUG_INFO
// #define DEBUG_TRACE

struct hid_input_button_skip
{
    uint16_t pid;
    uint8_t nr_skip;
};

/* Number of buttons in the HID descriptor to skip to find the buttons to map to 1/2/3/4.
 * If the pid of the controller is not listed here, the first 4 buttons found are used */
struct hid_input_button_skip controller_skip_buttons[] = {0x0268, 12}; /* For PS3, skip first 12 buttons */

#ifdef DEBUG_TRACE
#define PRINT_TRACE(...) { printf(__VA_ARGS__); }
#else
#define PRINT_TRACE(...) {}
#endif

#ifdef DEBUG_INFO
#define PRINT_INFO(...) { printf(__VA_ARGS__); }
#else
#define PRINT_INFO(...) {}
#endif

#if 0 // Descriptors for testing the parser
const uint8_t snes_descriptor[] = {
    0x05, 0x01, 0x09, 0x04, 0xA1, 0x01, 0xA1, 0x02, 0x75, 0x08, 0x95, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x35, 0x00, 0x46, 
    0xFF, 0x00, 0x09, 0x30, 0x09, 0x31, 0x81, 0x02, 0x95, 0x03, 0x81, 0x01, 0x75, 0x01, 0x95, 0x04, 0x15, 0x00, 0x25, 0x01, 
    0x35, 0x00, 0x45, 0x01, 0x81, 0x01, 0x65, 0x00, 0x75, 0x01, 0x95, 0x0A, 0x25, 0x01, 0x45, 0x01, 0x05, 0x09, 0x19, 0x01, 
    0x29, 0x0A, 0x81, 0x02, 0x06, 0x00, 0xFF, 0x75, 0x01, 0x95, 0x0A, 0x25, 0x01, 0x45, 0x01, 0x09, 0x01, 0x81, 0x02, 0xC0, 
    0xA1, 0x02, 0x75, 0x08, 0x95, 0x07, 0x46, 0xFF, 0x00, 0x26, 0xFF, 0x00, 0x09, 0x02, 0x91, 0x02, 0xC0, 0xC0 
};

const uint8_t ps3_descriptor[] = {
    0x05, 0x01, 0x09, 0x04, 0xA1, 0x01, 0xA1, 0x02, 0x85, 0x01, 0x75, 0x08, 0x95, 0x01, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x81, 
    0x03, 0x75, 0x01, 0x95, 0x13, 0x15, 0x00, 0x25, 0x01, 0x35, 0x00, 0x45, 0x01, 0x05, 0x09, 0x19, 0x01, 0x29, 0x13, 0x81, 
    0x02, 0x75, 0x01, 0x95, 0x0D, 0x06, 0x00, 0xFF, 0x81, 0x03, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x05, 0x01, 0x09, 0x01, 0xA1, 
    0x00, 0x75, 0x08, 0x95, 0x04, 0x35, 0x00, 0x46, 0xFF, 0x00, 0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35, 0x81, 0x02, 
    0xC0, 0x05, 0x01, 0x75, 0x08, 0x95, 0x27, 0x09, 0x01, 0x81, 0x02, 0x75, 0x08, 0x95, 0x30, 0x09, 0x01, 0x91, 0x02, 0x75, 
    0x08, 0x95, 0x30, 0x09, 0x01, 0xB1, 0x02, 0xC0, 0xA1, 0x02, 0x85, 0x02, 0x75, 0x08, 0x95, 0x30, 0x09, 0x01, 0xB1, 0x02, 
    0xC0, 0xA1, 0x02, 0x85, 0xEE, 0x75, 0x08, 0x95, 0x30, 0x09, 0x01, 0xB1, 0x02, 0xC0, 0xA1, 0x02, 0x85, 0xEF, 0x75, 0x08, 
    0x95, 0x30, 0x09, 0x01, 0xB1, 0x02, 0xC0, 0xC0
};

const uint8_t xbox_descriptor[] = {
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0xA1, 0x00, 0x09, 0x30, 0x09, 0x31, 0x15, 0x00, 0x27, 0xFF, 0xFF, 0x00, 0x00, 0x95, 0x02, 0x75, 0x10, 0x81, 0x02, 0xC0, 0xA1, 0x00, 0x09, 0x33, 0x09, 0x34, 
    0x15, 0x00, 0x27, 0xFF, 0xFF, 0x00, 0x00, 0x95, 0x02, 0x75, 0x10, 0x81, 0x02, 0xC0, 0x05, 0x01, 0x09, 0x32, 0x15, 0x00, 0x26, 0xFF, 0x03, 0x95, 0x01, 0x75, 0x0A, 0x81, 0x02, 0x15, 0x00, 0x25, 
    0x00, 0x75, 0x06, 0x95, 0x01, 0x81, 0x03, 0x05, 0x01, 0x09, 0x35, 0x15, 0x00, 0x26, 0xFF, 0x03, 0x95, 0x01, 0x75, 0x0A, 0x81, 0x02, 0x15, 0x00, 0x25, 0x00, 0x75, 0x06, 0x95, 0x01, 0x81, 0x03, 
    0x05, 0x09, 0x19, 0x01, 0x29, 0x0A, 0x95, 0x0A, 0x75, 0x01, 0x81, 0x02, 0x15, 0x00, 0x25, 0x00, 0x75, 0x06, 0x95, 0x01, 0x81, 0x03, 0x05, 0x01, 0x09, 0x39, 0x15, 0x01, 0x25, 0x08, 0x35, 0x00, 
    0x46, 0x3B, 0x01, 0x66, 0x14, 0x00, 0x75, 0x04, 0x95, 0x01, 0x81, 0x42, 0x75, 0x04, 0x95, 0x01, 0x15, 0x00, 0x25, 0x00, 0x35, 0x00, 0x45, 0x00, 0x65, 0x00, 0x81, 0x03, 0xA1, 0x02, 0x05, 0x0F, 
    0x09, 0x97, 0x15, 0x00, 0x25, 0x01, 0x75, 0x04, 0x95, 0x01, 0x91, 0x02, 0x15, 0x00, 0x25, 0x00, 0x91, 0x03, 0x09, 0x70, 0x15, 0x00, 0x25, 0x64, 0x75, 0x08, 0x95, 0x04, 0x91, 0x02, 0x09, 0x50, 
    0x66, 0x01, 0x10, 0x55, 0x0E, 0x26, 0xFF, 0x00, 0x95, 0x01, 0x91, 0x02, 0x09, 0xA7, 0x91, 0x02, 0x65, 0x00, 0x55, 0x00, 0x09, 0x7C, 0x91, 0x02, 0xC0, 0x05, 0x01, 0x09, 0x80, 0xA1, 0x00, 0x09, 
    0x85, 0x15, 0x00, 0x25, 0x01, 0x95, 0x01, 0x75, 0x01, 0x81, 0x02, 0x15, 0x00, 0x25, 0x00, 0x75, 0x07, 0x95, 0x01, 0x81, 0x03, 0xC0, 0x05, 0x06, 0x09, 0x20, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 
    0x08, 0x95, 0x01, 0x81, 0x02, 0xC0
};
#endif

/*
 * This is a very minimal and very hacky HID descriptor parser. But hopefully it is good enough to
 * handle most controllers.
 * It looks for the first Usage (X) and assigns that and the next control to be the X/Y axes
 * And for the first 4 buttons and assigns them as buttons (by default).
 * Controller pid can be listed in hid_input_button_skip to configure which controller buttons are used.
 * Ignores collections and other structural elements.
 *
 * Reads the HID report descriptor and populates joystick_definition with the parsed configuration values
 */
uint8_t parse_report_descriptor(uint16_t pid, uint8_t const *desc_report, uint16_t desc_len, struct joystick_bytes *joystick_definition)
{
    union TU_ATTR_PACKED
    {
        uint8_t byte;
        struct TU_ATTR_PACKED
        {
            uint8_t size : 2;
            uint8_t type : 2;
            uint8_t tag : 4;
        };
    } header;

    int bit_counter = 0; /* How many bits into the report descriptor is the current input control */

    // uint8_t report_num = 0;
    // uint8_t report_size = 0;
    // uint8_t report_count = 0;
    uint8_t nr_skip_buttons = 0;
    memset(joystick_definition, 0, sizeof(struct joystick_bytes));

    for (int i = 0; i < sizeof(controller_skip_buttons) / sizeof(struct hid_input_button_skip); i++)
    {
        if (pid == controller_skip_buttons[i].pid)
        {
            nr_skip_buttons = controller_skip_buttons[i].nr_skip;
        }
    }

    /* current parsed report count & size from descriptor */
    uint8_t ri_report_count = 0;
    uint8_t ri_report_size = 0;
    uint8_t ri_report_usage = 0;
    uint8_t ri_report_min = 0;
    uint8_t ri_report_max = 0;

    /* Skip the initial usage descriptor, it has already been checked */
    desc_report += 4;
    desc_len -= 4;

    while (desc_len > 0)
    {
        header.byte = *desc_report++;
        desc_len--;

        uint8_t const tag = header.tag;
        uint8_t const type = header.type;
        uint8_t const size = header.size;

        uint8_t const data8 = desc_report[0];

        PRINT_TRACE("[%02X] tag = %d, type = %d, size = %d, data = ", header.byte, tag, type, size);
        for (uint32_t i = 0; i < size; i++)
            PRINT_TRACE("%02X ", desc_report[i]);
        PRINT_TRACE("\r\n");

        switch (type)
        {
        case RI_TYPE_GLOBAL:
        {
            switch (tag)
            {
            case RI_GLOBAL_USAGE_PAGE:
            {
                if (data8 == HID_USAGE_PAGE_BUTTON)
                {
                    PRINT_TRACE("Usage Button\n");
                    ri_report_usage = HID_USAGE_PAGE_BUTTON;
                }
            }
            break;
            case RI_GLOBAL_LOGICAL_MIN:
            {
                PRINT_TRACE("min = %d\r\n", data8);
                ri_report_min = data8;
            }
            break;
            case RI_GLOBAL_LOGICAL_MAX:
                /* Note: For 16 bit controllers, we still assume 8 bit      */
                /* But this should still look at the most significant byte? */
                PRINT_TRACE("max = %d, bits = %d\r\n", data8, ri_report_size);
                ri_report_max = data8;
                break;
            case RI_GLOBAL_REPORT_ID:
                /* If ther is a report id, it will be the first byte of the report message */
                if (!joystick_definition->has_report_id)
                {
                    joystick_definition->has_report_id = 1;
                    joystick_definition->report_id = data8;
                    PRINT_TRACE("Report Id: %d\n", data8);
                }
                bit_counter += 8;
                break;
            case RI_GLOBAL_REPORT_SIZE:
                ri_report_size = data8;
                PRINT_TRACE("Report Size = %d\n", data8);
                break;

            case RI_GLOBAL_REPORT_COUNT:
                ri_report_count = data8;
                PRINT_TRACE("Report Count = %d\n", data8);
                break;
            }
        }
        break;
        case RI_TYPE_LOCAL:
        {
            switch (tag)
            {
            case RI_LOCAL_USAGE:
            {
                switch (data8)
                {
                case HID_USAGE_DESKTOP_X:
                {
                    PRINT_TRACE("Usage X\n");
                    ri_report_usage = data8;
                    break;
                }
                case HID_USAGE_DESKTOP_Y:
                {
                    PRINT_TRACE("Usage Y\n");
                    break;
                }
                }
                break;
            }
            }
        }
        break;
        case RI_TYPE_MAIN:
        {
            switch (tag)
            {
            case RI_MAIN_INPUT:
            {
                /* Input tag triggers registration of sticks / buttons */
                for (int i = 0; i < ri_report_count; i++)
                {
                    PRINT_TRACE("Input: Byte[%d:%d]\n", bit_counter / 8, bit_counter % 8);
                    if (ri_report_usage == HID_USAGE_PAGE_BUTTON &&
                        ri_report_size == 1 &&
                        data8 == 0x02) /* Data input */
                    {
                        if (nr_skip_buttons > 0)
                        {
                            PRINT_TRACE("Skipping Button\n");
                            nr_skip_buttons--;
                            bit_counter += ri_report_size;
                            continue;
                        }
                        uint8_t mask = 1 << (bit_counter % 8);
                        if (!joystick_definition->b1_set)
                        {
                            joystick_definition->buttons_byte = bit_counter / 8;
                            joystick_definition->b1_mask = mask;
                            joystick_definition->b1_set = 1;
                        }
                        else if (!joystick_definition->b2_set)
                        {
                            joystick_definition->b2_mask = mask;
                            joystick_definition->b2_set = 1;
                        }
                        else if (!joystick_definition->b3_set)
                        {
                            joystick_definition->b3_mask = mask;
                            joystick_definition->b3_set = 1;
                        }
                        else if (!joystick_definition->b4_set)
                        {
                            joystick_definition->b4_mask = mask;
                            joystick_definition->b4_set = 1;
                        }
                    }
                    else if (!joystick_definition->xy_set &&
                             (ri_report_usage == HID_USAGE_DESKTOP_X ||
                              ri_report_usage == HID_USAGE_DESKTOP_Y))
                    {
                        if (ri_report_min != 0 || ri_report_max != 255 || ri_report_size != 8)
                        {
                            PRINT_INFO("Joystick probably not supported: Min[0:%d], Max[255:%d], Size[8:%d]\n",
                                       ri_report_min, ri_report_max, ri_report_size);
                        }
                        /* Assumes the Y follows the X and is 8 bit with a min/max of 255*/
                        joystick_definition->x_axis_byte = bit_counter / 8;
                        joystick_definition->y_axis_byte = joystick_definition->x_axis_byte + (ri_report_size / 8);
                        joystick_definition->xy_set = 1;
                    }
                    bit_counter += ri_report_size;
                }
                break;
            }
            }
        }
        break;
        }
        desc_report += size;
        desc_len -= size;
    }
    PRINT_INFO("JOYSTICK_DEFINITION\n");
    if (joystick_definition->has_report_id)
    {
        PRINT_INFO("report id: %d\n", joystick_definition->report_id);
    }
    PRINT_INFO("x offset: %d\n", joystick_definition->x_axis_byte);
    PRINT_INFO("y offset: %d\n", joystick_definition->y_axis_byte);
    PRINT_INFO("b offset: %d\n", joystick_definition->buttons_byte);
    PRINT_INFO("b1 mask: %x\n", joystick_definition->b1_mask);
    PRINT_INFO("b2 mask: %x\n", joystick_definition->b2_mask);
    PRINT_INFO("b3 mask: %x\n", joystick_definition->b3_mask);
    PRINT_INFO("b4 mask: %x\n", joystick_definition->b4_mask);

    return joystick_definition->xy_set &&
           joystick_definition->b1_set &&
           joystick_definition->b2_set &&
           joystick_definition->b3_set &&
           joystick_definition->b4_set;
}

#if 0 // For testing the parser

void main()
{
    uint8_t val;
    int16_t sval;

    val = 0;
    sval = val;
    printf("Top: %d\n", (int8_t)get_joy_value_y(val));
    val = 127;
    sval = val;
    printf("Middle: %d\n", (int8_t)get_joy_value_y(val));
    val = 255;
    sval = val;
     printf("Bottom: %d\n", (int8_t)get_joy_value_y(val));

    struct joystick_bytes def;
    //parse_report_descriptor(0, snes_descriptor, sizeof(snes_descriptor), &def);

    //memset(&def, 0, sizeof(struct joystick_bytes));
    //parse_report_descriptor(0x0268, ps3_descriptor, sizeof(ps3_descriptor), &def);

//    memset(&def, 0, sizeof(struct joystick_bytes));
//    parse_report_descriptor(xbox_descriptor, sizeof(xbox_descriptor), &def);

}
#endif