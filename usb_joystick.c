#include "usb_joystick.h"
#include "bsp/board.h"
#include "tusb.h"
#include <string.h>

/* Commands sent to Altair-duino */
#define DAZ_JOY1      0x10
#define DAZ_JOY2      0x20
#define DAZ_KEY       0x30

/* Support 2 joysticks */
static usb_joystick joysticks[2];

/* Global used when calling hid_set_report so can block until result is received */
static uint16_t hid_report_status = -1;

void usb_send_bytes(uint8_t *buf, int count);
void process_joy_input(int joynum, usb_joystick* joy);
bool tuh_hid_receive_report(uint8_t dev_addr, uint8_t instance);

/* Return true if PS3 controller is connected. This controller needs 
 * additional USB commands to enable it */
bool is_ps3_controller(uint16_t pid)
{
    return (pid == 0x0268);
}

/* Callback when Invoked when device with hid interface is mounted */
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);
    
    /* Usage page values for Joystick and Gamepad */
    uint8_t joystick_hid[4] = { 0x05, 0x01, 0x09, 0x04 };
    uint8_t gamepad_hid[4] = { 0x05, 0x01, 0x09, 0x05 };

    printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);
    printf("Instance = %d, VID = %04x, PID = %04x\r\n", instance, vid, pid);

#ifdef DEBUG
    for (int i = 0 ; i < desc_len ; i++)
    {
        printf("%02X ", desc_report[i]);
        if ((i % 20) == 19) printf("\r\n");
    }
    printf("\r\n");
#endif

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
                        while (hid_report_status == -1)
                        {
                            /* wait for response from set report*/
                            tuh_task();
                        }
                        if (hid_report_status == 0)
                        {
                            printf ("ERROR configuring PS3 controller\n");
                        }
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
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
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

//#define DEBUGXXX
/* Invoked when received report from device via interrupt endpoint */
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
//    printf("tuh_hid_report_received_cb\n");
    for (int i = 0 ; i < 2 ; i++)
    {

      /* check if joystick is connected and values have changed since last report */
      if(joysticks[i].dev_addr == dev_addr &&
         joysticks[i].instance == instance &&
         joysticks[i].connected &&
        /* If there are multiple report types, the reportid is assumed to be the first byte */
         (!joysticks[i].offsets.has_report_id || joysticks[i].offsets.report_id == report[0]) &&
         (joysticks[i].prev_x != report[joysticks[i].offsets.x_axis_byte] ||
         joysticks[i].prev_y != report[joysticks[i].offsets.y_axis_byte] ||
         joysticks[i].prev_buttons != report[joysticks[i].offsets.buttons_byte]))
      {
          joysticks[i].x = report[joysticks[i].offsets.x_axis_byte];
          joysticks[i].y = report[joysticks[i].offsets.y_axis_byte];
          uint8_t buttons = report[joysticks[i].offsets.buttons_byte];
          joysticks[i].buttons = buttons;
          joysticks[i].b1 = buttons & joysticks[i].offsets.b1_mask;
          joysticks[i].b2 = buttons & joysticks[i].offsets.b2_mask;
          joysticks[i].b3 = buttons & joysticks[i].offsets.b3_mask;
          joysticks[i].b4 = buttons & joysticks[i].offsets.b4_mask;
          
          process_joy_input(i, &joysticks[i]);
          joysticks[i].prev_x = report[joysticks[i].offsets.x_axis_byte];
          joysticks[i].prev_y = report[joysticks[i].offsets.y_axis_byte];
          joysticks[i].prev_buttons = report[joysticks[i].offsets.buttons_byte];
          break;
        }
    }
}

/*
 * Request to receive a HID report from the joystick. 
 */
void schedule_joy_input()
{
    for (int i = 0 ; i < 2 ; i++)
    {
        if (joysticks[i].connected)
        {
            bool result = tuh_hid_receive_report(joysticks[i].dev_addr, joysticks[i].instance);
#ifdef DEBUG
            printf("tuh_hid_receive_report(%d, %d) = %d\n", joysticks[i].dev_addr, joysticks[i].instance, result);
#endif
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


uint8_t get_joy_value_x (uint8_t value)
{
    int16_t sval = value;
    sval -= 127;
    if (sval == 128) sval -= 2;
    return (uint8_t) sval;
}

uint8_t get_joy_value_y (uint8_t value)
{
    int16_t sval = value;
    sval -= 127;
    sval = -sval;
    if (sval == -128) sval += 1;
    return (uint8_t) sval;
}

/* Send joystick input to Altair-duino */
void process_joy_input(int joynum, usb_joystick* joy)
{
    uint8_t daz_msg[3];
    daz_msg[0] = (joynum == 0) ? DAZ_JOY1 : DAZ_JOY2;
    if (!joy->b1) daz_msg[0] |= 1;
    if (!joy->b2) daz_msg[0] |= 2;
    if (!joy->b3) daz_msg[0] |= 4;
    if (!joy->b4) daz_msg[0] |= 8;
    daz_msg[1] = get_joy_value_x(joy->x);
    daz_msg[2] = get_joy_value_y(joy->y);
    
 //   printf ("Sending joystick report\r\n");
#ifdef DEBUGXXX
    printf("Joy = %d, X = %d, Y = %d, btn = %x, msg[0] = %02x\r\n", joynum, (int8_t) daz_msg[1], (int8_t) daz_msg[2], daz_msg[0] & 0x0F, daz_msg[0]);
#endif
    usb_send_bytes(daz_msg, 3);
}
