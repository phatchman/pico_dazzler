/*
 * The MIT License (MIT)
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
#include "bsp/board.h"
#include "tusb.h"
#include "hid_devices.h"
#include "usb_joystick.h"
#include "usb_kbd.h"

#define DEBUG_INFO  (DEBUG_JOYSTICK + DEBUG_KEYOARD)
#define DEBUG_TRACE (TRACE_JOYSTICK + TRACE_KEYBOARD)
#include "debug.h"

/* Invoked when hid device is mounted */
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
    printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);

    // Interface protocol (hid_interface_protocol_enum_t)
    const char* protocol_str[] = { "None", "Keyboard", "Mouse" };
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    printf("HID Interface Protocol = %s\r\n", protocol_str[itf_protocol]);
 
 #if DEBUG_INFO > 0
    for (int i = 0 ; i < desc_len ; i++)
    {
        printf("%02X ", desc_report[i]);
        if ((i % 20) == 19) printf("\n");
    }
    printf("\n");
#endif

    switch(itf_protocol)
    {
        case HID_ITF_PROTOCOL_KEYBOARD:
            kbd_hid_mount_cb(dev_addr, instance, desc_report, desc_len);
        case HID_ITF_PROTOCOL_MOUSE:
            break;
        default:
            joy_hid_mount_cb(dev_addr, instance, desc_report, desc_len);
    }
}

/* Invoked when HID device is unmounted */
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
        switch(itf_protocol)
    {
        case HID_ITF_PROTOCOL_KEYBOARD:
            kbd_hid_unmount_cb(dev_addr, instance);
        case HID_ITF_PROTOCOL_MOUSE:
            break;
        default:
            joy_hid_unmount_cb(dev_addr, instance);
    }
}

/* Invoked when received report from device via interrupt endpoint */
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    PRINT_TRACE("HID Report Received for %d:%d:%d\n", dev_addr, instance, itf_protocol);
    switch (itf_protocol)
    {
        case HID_ITF_PROTOCOL_KEYBOARD:
            kbd_process_hid_report(dev_addr, instance, report, len);
            break;

        case HID_ITF_PROTOCOL_MOUSE:
            break;

        default:
            joy_process_hid_report(dev_addr, instance, report, len);
        break;
    }
}

/* Poll connected USB devices for input */
void hid_schedule_device_poll(void)
{
    joy_schedule_hid_input();
    kbd_schedule_hid_input();
}