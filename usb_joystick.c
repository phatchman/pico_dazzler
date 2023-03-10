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

/* TODO: There is some issue if the keyboard is detected before the PS3 controller 
 * which does not occur with my non-PS3 controller
 * The wrong descriptor seems to be passed in to the mounted callback. It's not a valid
 * HID desciptor, so the PS3 isn't detected as a joystick. The workaround is to plug the PS3
 * controller into an earlier port than the keyboard.
 * Proably a bug in TinyUSB?
 */
#include "bsp/board.h"
#include "tusb.h"
#include <string.h>
#include "usb_joystick.h"


#define DEBUG_INFO  DEBUG_JOYSTICK
#define DEBUG_TRACE TRACE_JOYSTICK
#include "debug.h"

/* Commands sent to Altair-duino */
#define DAZ_JOY1      0x10
#define DAZ_JOY2      0x20
#define DAZ_KEY       0x30

/* Support 2 joysticks */
static usb_joystick joysticks[2];

/* Global used when calling hid_set_report so can block until result is received */
static uint16_t hid_report_status = -1;

void usb_send_bytes(uint8_t *buf, int count);
static void joy_process_input(int joynum, usb_joystick* joy);

/* Return true if PS3 controller is connected. This controller needs 
 * additional USB commands to enable it */
bool is_ps3_controller(uint16_t pid)
{
    return (pid == 0x0268);
}

bool is_xbox_controller(uint16_t pid)
{
    switch(pid)
    {
        case 0x028E: // Microsoft 360 Wired controller
        case 0x0719: // Microsoft Wireless Gaming Receiver
        case 0x0291: // Third party Wireless Gaming Receiver
        case 0xF016: // Mad Catz wired controlle
        case 0xBEEF: // For Joytech wired controller
        case 0x0401: // Gamestop wired controller
        case 0x0213: // Afterglow wired controller
        case 0x02D1: // Microsoft X-Box One pad
        case 0x02DD: // Microsoft X-Box One pad (Firmware 2015)
        case 0x02E3: // Microsoft X-Box One Elite pad
        case 0x02EA: // Microsoft X-Box One S pad
        case 0x0B0A: // Microsoft X-Box One Adaptive Controller
        case 0x0B12: // Microsoft X-Box Core Controller
        case 0x4A01: // Mad Catz FightStick TE 2
        case 0x0139: // Afterglow Prismatic Wired Controller
        case 0x0146: // Rock Candy Wired Controller for Xbox One
        case 0x0067: // HORIPAD ONE
        case 0x0A03: // Razer Wildcat
        case 0x541A: // PowerA Xbox One Mini Wired Controller
        case 0x542A: // Xbox ONE spectra
        case 0x543A: // PowerA Xbox One wired controller
            return true;
        default:
            return false;

    }
    return (pid == 0x02e3);
}

/* Callback when Invoked when device with hid interface type of "None" is mounted
 * Check if this is a joystick or gamepad and initialize it */
void joy_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);
    
    /* Usage page values for Joystick and Gamepad */
    uint8_t joystick_hid[4] = { 0x05, 0x01, 0x09, 0x04 };
    uint8_t gamepad_hid[4] = { 0x05, 0x01, 0x09, 0x05 };

    printf("Instance = %d, VID = %04x, PID = %04x\n", instance, vid, pid);

    if (!memcmp(desc_report, joystick_hid, sizeof (joystick_hid)) ||
        !memcmp(desc_report, gamepad_hid, sizeof (gamepad_hid)))
    {
        printf("JOYSTICK or GAMEPAD connected\r\n");
        for (int i = 0 ; i < 2 ; i++)
        {
            if (!joysticks[i].connected)
            {
                memset(&joysticks[i], 0, sizeof(usb_joystick));

                joysticks[i].dev_addr = dev_addr;
                joysticks[i].instance = instance;
                joysticks[i].dead_zone = 8;
                if (parse_report_descriptor(pid, desc_report, desc_len, &joysticks[i].offsets))
                {
                    joysticks[i].connected = true;
                    printf("Connected as joystick %d\n", i);
                    if (is_ps3_controller(pid))
                    {
                        /* PS3 controller needs a command to tell it to start sending reports */
                        static  uint8_t cmd_buf[] = { 0x42, 0x0c, 0x00, 0x00 };
                        printf("SENDING PS3 REPORT\r\n");
                        
                        hid_report_status = -1;
                        tuh_hid_set_report(dev_addr, instance, 0xF4, 
                                            HID_REPORT_TYPE_FEATURE, cmd_buf, sizeof(cmd_buf));
#if 0       /* Waiting for the report result causes issues when keyboard is connected. Get  
             * a Data Sequence error. Not clear why. But no real need to wait for the report result.
                        while (hid_report_status == -1)
                        {
                            /* wait for response from set report*/
                            tuh_task();
                        }
                        if (hid_report_status == 0)
                        {
                            printf ("ERROR configuring PS3 controller\n");
                        }
#endif
                    }
                    else if (is_xbox_controller(pid))
                    {
                        printf("SENDING XBOX ENABLE\r\n");
                        static uint8_t cmd_buf[] = { 0x05, 0x20, 0x00, 0x01, 0x00 };
                        static tuh_xfer_t xfer = 
                        {
                            .ep_addr = 0x01,
                            .buflen = sizeof(cmd_buf),
                            .buffer = cmd_buf,
                            .complete_cb = NULL,
                            .user_data = 0
                        };
                        xfer.daddr = dev_addr;
                  
                        tuh_edpt_xfer(&xfer);
                        joysticks[i].zero_centered = true;
                        /* My XBOX elite controller seems to have really bad centering */
                        joysticks[i].dead_zone = 16;
                        printf("SENT XBOX REPORT\n");
                    }
                }
                else
                {
                    printf("Error parsing joystick HID report descriptor\n");
                }
                break;      
            }
        }
    }
}

/* Invoked when device with hid interface is un-mounted */
void joy_hid_unmount_cb(uint8_t dev_addr, uint8_t instance)
{
  for (int i = 0 ; i < 2 ; i++)
  {
    if (joysticks[i].dev_addr == dev_addr && joysticks[i].instance == instance)
    {
      printf("Disconnecting joystick %d\n", i);
      joysticks[i].connected = false;
      break;
    }
  }
}


/* Invoked when received report from device via interrupt endpoint */
void joy_process_hid_report(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
    PRINT_TRACE("process_joystick_report\n");
    static absolute_time_t swap_time;
    static int swapping_joy = -1;   /* Joystick no that is initiating swap */
    static bool swapped = false;    /* True if swapped, but buttons not yet released */
    for (int i = 0 ; i < 2 ; i++)
    {
        usb_joystick* joy = &joysticks[i];
        /* check if joystick is connected and values have changed since last report */
        if(joy->dev_addr == dev_addr &&
           joy->instance == instance &&
           joy->connected)
        {
            /* If there are multiple report types, the reportid is assumed to be the first byte */
            if ((!joy->offsets.has_report_id || joy->offsets.report_id == report[0]) &&
                (joy->prev_x != report[joy->offsets.x_axis_byte] ||
                joy->prev_y != report[joy->offsets.y_axis_byte] ||
                joy->prev_buttons != report[joy->offsets.buttons_byte]))
            {
                joy->x = report[joy->offsets.x_axis_byte];
                joy->y = report[joy->offsets.y_axis_byte];
                uint8_t buttons = report[joy->offsets.buttons_byte];
                joy->buttons = buttons;
                joy->b1 = buttons & joy->offsets.b1_mask;
                joy->b2 = buttons & joy->offsets.b2_mask;
                joy->b3 = buttons & joy->offsets.b3_mask;
                joy->b4 = buttons & joy->offsets.b4_mask;
            
                joy_process_input(i, &joysticks[i]);
                joysticks[i].prev_x = report[joysticks[i].offsets.x_axis_byte];
                joysticks[i].prev_y = report[joysticks[i].offsets.y_axis_byte];
                joysticks[i].prev_buttons = report[joysticks[i].offsets.buttons_byte];
            }
            /*
            * Can swap Joysticks by holding all 4 buttons for 2 seconds
            */
            if (joy->b1 && joy->b2 && joy->b3 && joy->b4)
            {
                absolute_time_t now = get_absolute_time();
                if (swapping_joy == -1 && !swapped)  /* Swap not initiated and not finishing a swap */
                {
                    swapping_joy = i;
                    swap_time = delayed_by_ms(now, 2000);
                }
                else if (swapping_joy == i && !swapped) /* Swap already initiated on this joystick */
                {
                    if (absolute_time_diff_us(swap_time, now) >= 0)
                    {
                        printf("SWAPPING JOYSTICKS\n");
                        usb_joystick swapped_joy[2];
                        memcpy(&swapped_joy[0], &joysticks[1], sizeof(usb_joystick));
                        memcpy(&swapped_joy[1], &joysticks[0], sizeof(usb_joystick));
                        memcpy(joysticks, swapped_joy, sizeof(swapped_joy));
                        swapped = true;
                        /* Clear button presses on other joystick */
                        if (!joysticks[swapping_joy].connected)
                        {
                            memset(&joysticks[swapping_joy], 0, sizeof(usb_joystick));
                        }
                        joy_process_input(swapping_joy, &joysticks[swapping_joy]);
                        swapping_joy = (swapping_joy == 0) ? 1 : 0;
                        /* Send update for initiating joystick */
                        joy_process_input(swapping_joy, &joysticks[swapping_joy]);
                    }
                }
            }
            else if (swapping_joy == i) /* Buttons released on swapping joystick */
            {
                swapping_joy = -1;
                swapped = false;
            }
            /* Report is for a single joystick. Don't keep looking */
            break;         
        }
    }
}

/*
 * Request to receive a HID report from the joystick. 
 */
void joy_schedule_hid_input(void)
{
    for (int i = 0 ; i < 2 ; i++)
    {
        if (joysticks[i].connected)
        {
            bool result = tuh_hid_receive_report(joysticks[i].dev_addr, joysticks[i].instance);
            PRINT_TRACE("tuh_hid_receive_report(%d, %d) = %d\n", joysticks[i].dev_addr, joysticks[i].instance, result);
        }
    }
}
  
/*
 * HID Set reports are async. setting status to not be -1 indicates that status send is complete.
 * A non-zero value in hid_report_status indicates success.
 */
void tuh_hid_set_report_complete_cb(uint8_t dev_addr, uint8_t instance, uint8_t report_id, 
      uint8_t report_type, uint16_t len)
{
    hid_report_status = len;
}


static uint8_t joy_get_value_x (usb_joystick* joy)
{
    int16_t sval = joy->zero_centered ? (int8_t) joy->x : joy->x;
    if (!joy->zero_centered)
    {
        sval -= 127;
        // Actually need to -128 for the +ve direction
        if (sval == 128) sval -= 1;
    }
    if (sval > -joy->dead_zone && sval < joy->dead_zone)
        sval = 0;
    /* Value of -128 reverses direction in some games */
    if (sval == -128) sval += 1;

    return (uint8_t) sval;
}

static uint8_t joy_get_value_y (usb_joystick* joy)
{
    int16_t sval = joy->zero_centered ? (int8_t) joy->y : joy->y;
    if (!joy->zero_centered)
    {
        sval -= 127;
        sval = -sval;
    }
     /* Value of -128 reverses direction in some games */   
    if (sval == -128) sval += 1;
    if (sval > -joy->dead_zone && sval < joy->dead_zone)
        sval = 0;

    return (uint8_t) sval;
}

/* Send joystick input to Altair-duino */
static void joy_process_input(int joynum, usb_joystick* joy)
{
    uint8_t daz_msg[3];
    daz_msg[0] = (joynum == 0) ? DAZ_JOY1 : DAZ_JOY2;
    if (!joy->b1) daz_msg[0] |= 1;
    if (!joy->b2) daz_msg[0] |= 2;
    if (!joy->b3) daz_msg[0] |= 4;
    if (!joy->b4) daz_msg[0] |= 8;

    daz_msg[1] = joy_get_value_x(joy);
    daz_msg[2] = joy_get_value_y(joy);

    PRINT_INFO("Joy = %d, X = %d, Y = %d, btn = %x, msg[0] = %02x\r\n", joynum, (int8_t) daz_msg[1], (int8_t) daz_msg[2], daz_msg[0] & 0x0F, daz_msg[0]);
    usb_send_bytes(daz_msg, 3);
}
