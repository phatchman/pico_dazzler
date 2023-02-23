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

#define DEBUG_INFO  DEBUG_KEYBOARD
#define DEBUG_TRACE TRACE_KEYBOARD
#include "debug.h"       

/* Commands sent to Altair-duino */
#define DAZ_KEY     0x30
void usb_send_bytes(uint8_t *buf, int count);

/* TODO: Handle CAPS LOCK */

/*--------------------------------------------------------------------
 * KEYCODE to Ascii Conversion
 * Expand to array of [128][3] (ascii without shift, with shift, with ctrl)
 * Assumes US keyboard layout (apologies to rest of world)
 *--------------------------------------------------------------------*/
#define DAZ_HID_KEYCODE_TO_ASCII    \
    {0     , 0      , 0     }, /* 0x00 */ \
    {0     , 0      , 0     }, /* 0x01 */ \
    {0     , 0      , 0     }, /* 0x02 */ \
    {0     , 0      , 0     }, /* 0x03 */ \
    {'a'   , 'A'    , 1     }, /* 0x04 */ \
    {'b'   , 'B'    , 2     }, /* 0x05 */ \
    {'c'   , 'C'    , 3     }, /* 0x06 */ \
    {'d'   , 'D'    , 4     }, /* 0x07 */ \
    {'e'   , 'E'    , 5     }, /* 0x08 */ \
    {'f'   , 'F'    , 6     }, /* 0x09 */ \
    {'g'   , 'G'    , 7     }, /* 0x0a */ \
    {'h'   , 'H'    , 8     }, /* 0x0b */ \
    {'i'   , 'I'    , 9     }, /* 0x0c */ \
    {'j'   , 'J'    , 10    }, /* 0x0d */ \
    {'k'   , 'K'    , 11    }, /* 0x0e */ \
    {'l'   , 'L'    , 12    }, /* 0x0f */ \
    {'m'   , 'M'    , 13    }, /* 0x10 */ \
    {'n'   , 'N'    , 14    }, /* 0x11 */ \
    {'o'   , 'O'    , 15    }, /* 0x12 */ \
    {'p'   , 'P'    , 16    }, /* 0x13 */ \
    {'q'   , 'Q'    , 17    }, /* 0x14 */ \
    {'r'   , 'R'    , 18    }, /* 0x15 */ \
    {'s'   , 'S'    , 19    }, /* 0x16 */ \
    {'t'   , 'T'    , 20    }, /* 0x17 */ \
    {'u'   , 'U'    , 21    }, /* 0x18 */ \
    {'v'   , 'V'    , 22    }, /* 0x19 */ \
    {'w'   , 'W'    , 23    }, /* 0x1a */ \
    {'x'   , 'X'    , 24    }, /* 0x1b */ \
    {'y'   , 'Y'    , 25    }, /* 0x1c */ \
    {'z'   , 'Z'    , 26    }, /* 0x1d */ \
    {'1'   , '!'    , 0     }, /* 0x1e */ \
    {'2'   , '@'    , 0     }, /* 0x1f */ \
    {'3'   , '#'    , 0     }, /* 0x20 */ \
    {'4'   , '$'    , 0     }, /* 0x21 */ \
    {'5'   , '%'    , 0     }, /* 0x22 */ \
    {'6'   , '^'    , 0     }, /* 0x23 */ \
    {'7'   , '&'    , 0     }, /* 0x24 */ \
    {'8'   , '*'    , 0     }, /* 0x25 */ \
    {'9'   , '('    , 0     }, /* 0x26 */ \
    {'0'   , ')'    , 0     }, /* 0x27 */ \
    {'\r'  , '\r'   , 0     }, /* 0x28 */ \
    {'\x1b', '\x1b' , 0     }, /* 0x29 */ \
    {'\b'  , '\b'   , 0     }, /* 0x2a */ \
    {'\t'  , '\t'   , 0     }, /* 0x2b */ \
    {' '   , ' '    , 0     }, /* 0x2c */ \
    {'-'   , '_'    , 0     }, /* 0x2d */ \
    {'='   , '+'    , 0     }, /* 0x2e */ \
    {'['   , '{'    , 0     }, /* 0x2f */ \
    {']'   , '}'    , 0     }, /* 0x30 */ \
    {'\\'  , '|'    , 0     }, /* 0x31 */ \
    {'#'   , '~'    , 0     }, /* 0x32 */ \
    {';'   , ':'    , 0     }, /* 0x33 */ \
    {'\''  , '\"'   , 0     }, /* 0x34 */ \
    {'`'   , '~'    , 0     }, /* 0x35 */ \
    {','   , '<'    , 0     }, /* 0x36 */ \
    {'.'   , '>'    , 0     }, /* 0x37 */ \
    {'/'   , '?'    , 0     }, /* 0x38 */ \
                                          \
    {0     , 0      , 0     }, /* 0x39 */ \
    {0     , 0      , 0     }, /* 0x3a */ \
    {0     , 0      , 0     }, /* 0x3b */ \
    {0     , 0      , 0     }, /* 0x3c */ \
    {0     , 0      , 0     }, /* 0x3d */ \
    {0     , 0      , 0     }, /* 0x3e */ \
    {0     , 0      , 0     }, /* 0x3f */ \
    {0     , 0      , 0     }, /* 0x40 */ \
    {0     , 0      , 0     }, /* 0x41 */ \
    {0     , 0      , 0     }, /* 0x42 */ \
    {0     , 0      , 0     }, /* 0x43 */ \
    {0     , 0      , 0     }, /* 0x44 */ \
    {0     , 0      , 0     }, /* 0x45 */ \
    {0     , 0      , 0     }, /* 0x46 */ \
    {0     , 0      , 0     }, /* 0x47 */ \
    {0     , 0      , 0     }, /* 0x48 */ \
    {0     , 0      , 0     }, /* 0x49 */ \
    {0     , 0      , 0     }, /* 0x4a */ \
    {0     , 0      , 0     }, /* 0x4b */ \
    {0     , 0      , 0     }, /* 0x4c */ \
    {0     , 0      , 0     }, /* 0x4d */ \
    {0     , 0      , 0     }, /* 0x4e */ \
    {0     , 0      , 0     }, /* 0x4f */ \
    {0     , 0      , 0     }, /* 0x50 */ \
    {0     , 0      , 0     }, /* 0x51 */ \
    {0     , 0      , 0     }, /* 0x52 */ \
    {0     , 0      , 0     }, /* 0x53 */ \
                                          \
    {'/'   , '/'    , 0     }, /* 0x54 */ \
    {'*'   , '*'    , 0     }, /* 0x55 */ \
    {'-'   , '-'    , 0     }, /* 0x56 */ \
    {'+'   , '+'    , 0     }, /* 0x57 */ \
    {'\r'  , '\r'   , 0     }, /* 0x58 */ \
    {'1'   , 0      , 0     }, /* 0x59 */ \
    {'2'   , 0      , 0     }, /* 0x5a */ \
    {'3'   , 0      , 0     }, /* 0x5b */ \
    {'4'   , 0      , 0     }, /* 0x5c */ \
    {'5'   , '5'    , 0     }, /* 0x5d */ \
    {'6'   , 0      , 0     }, /* 0x5e */ \
    {'7'   , 0      , 0     }, /* 0x5f */ \
    {'8'   , 0      , 0     }, /* 0x60 */ \
    {'9'   , 0      , 0     }, /* 0x61 */ \
    {'0'   , 0      , 0     }, /* 0x62 */ \
    {'.'   , 0      , 0     }, /* 0x63 */ \
    {0     , 0      , 0     }, /* 0x64 */ \
    {0     , 0      , 0     }, /* 0x65 */ \
    {0     , 0      , 0     }, /* 0x66 */ \
    {'='   , '='    , 0     }, /* 0x67 */ \


static uint8_t const keycode2ascii[128][3] =  { DAZ_HID_KEYCODE_TO_ASCII };
static struct {
    uint8_t dev_addr; 
    uint8_t instance;
    bool    connected;
} keyboard_device;


void kbd_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
    if (!keyboard_device.connected)
    {
        keyboard_device.dev_addr = dev_addr;
        keyboard_device.instance = instance;
        keyboard_device.connected = true;
    }
}

void kbd_hid_unmount_cb(uint8_t dev_addr, uint8_t instance)
{
    if (keyboard_device.connected &&
        keyboard_device.dev_addr == dev_addr &&
        keyboard_device.instance == instance)
    {
        keyboard_device.connected = false;
    }
}

void kbd_schedule_hid_input(void)
{
    if (keyboard_device.connected)
    {
        bool result = tuh_hid_receive_report(keyboard_device.dev_addr, keyboard_device.instance);
        PRINT_TRACE("tuh_hid_receive_report(%d, %d) = %d\n", keyboard_device.dev_addr, keyboard_device.instance, result);
    }
}

// look up new key in previous keys
static inline bool find_key_in_report(hid_keyboard_report_t const *report, uint8_t keycode)
{
    for(uint8_t i=0; i<6; i++)
    {
        if (report->keycode[i] == keycode)
            return true;
    }

    return false;
}

void kbd_process_hid_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
    static hid_keyboard_report_t prev_kb_report = { 0, 0, {0} }; // previous report to check key released
    hid_keyboard_report_t const *kb_report = (hid_keyboard_report_t *) report;

    for(uint8_t i=0; i<6; i++)
    {
        if (kb_report->keycode[i])
        {
            if (find_key_in_report(&prev_kb_report, kb_report->keycode[i]))
            {
                // TODO implement key repeat here?
                // exist in previous report means the current key is holding
            }
            else
            {
                int modifier = 0;   // 0 = normal, 1 = shift 2 = ctrl
                // not existed in previous report means the current key is pressed
                if (kb_report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT))
                {
                    modifier = 1;
                }
                else if (kb_report->modifier & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL))
                {
                    modifier = 2;
                }

                uint8_t ch = keycode2ascii[kb_report->keycode[i]][modifier];
#if DEBUG_TRACE > 0
                putchar(ch);
                if ( ch == '\r' ) putchar('\n'); // added new line for enter key

                fflush(stdout); // flush right away, else nanolib will wait for newline
#endif
                uint8_t msg[2];
                msg[0] = DAZ_KEY;
                msg[1] = ch;
                usb_send_bytes(msg, 2);
            }
        }
    }

    prev_kb_report = *kb_report;
}