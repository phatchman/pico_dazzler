#include "usb_joystick.h"

#include "bsp/board.h"
#include "tusb.h"

#include <string.h>




#define DAZ_JOY1      0x10
#define DAZ_JOY2      0x20
#define DAZ_KEY       0x30

static usb_joystick joysticks[2];

static bool joy_mounted = false;

/* TODO: this is defined in main. Also it is a dumb name for this */
void usb_send_byte(uint8_t *buf, int count);
void process_joy_input(uint8_t joynum, const uint8_t* report, uint8_t len);
bool tuh_hid_receive_report(uint8_t dev_addr, uint8_t instance);

// Invoked when device with hid interface is mounted
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);
  uint8_t joystick_hid[4] = { 0x05, 0x01, 0x09, 0x04 };
  uint8_t gamepad_hid[4] = { 0x05, 0x01, 0x09, 0x05 };

  printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);
  printf("Instance = %d, VID = %04x, PID = %04x\r\n", instance, vid, pid);
  for (int i = 0 ; i < desc_len ; i++)
  {
    printf("%02X ", desc_report[i]);
    if ((i % 20) == 19) printf("\r\n");
  }
  printf("\r\n");

  if (!memcmp(desc_report, joystick_hid, sizeof (joystick_hid)) ||
    !memcmp(desc_report, gamepad_hid, sizeof (gamepad_hid)))
  {
    printf("JOYSTICK or GAMEPAD connected\r\n");
    for (int i = 0 ; i < 2 ; i++)
    {
      if (!joysticks[i].connected)
      {
        joysticks[i].dev_addr = dev_addr;
        joysticks[i].instance = instance;
        memset(&joysticks[i].prev_report, 0, sizeof(joystick_report));
        joysticks[i].connected = true;
        printf("Connected as joystick %d\n", i);
        joy_mounted = true;
        break;      
      }
    }
  }

#ifdef NOTUSED
  void PS3USB::enable_sixaxis() { // Command used to enable the Dualshock 3 and Navigation controller to send data via USB
        uint8_t cmd_buf[4];
        cmd_buf[0] = 0x42; // Special PS3 Controller enable commands
        cmd_buf[1] = 0x0c;
        cmd_buf[2] = 0x00;
        cmd_buf[3] = 0x00;

        // bmRequest = Host to device (0x00) | Class (0x20) | Interface (0x01) = 0x21, 
        // bRequest = Set Report (0x09), Report ID (0xF4), Report Type (Feature 0x03), 
        // interface (0x00), datalength, datalength, data)
        pUsb->ctrlReq(bAddress, epInfo[PS3_CONTROL_PIPE].epAddr, bmREQ_HID_OUT, HID_REQUEST_SET_REPORT, 
        0xF4, 0x03, 0x00, 4, 4, cmd_buf, NULL);
#endif
}

// Invoked when device with hid interface is un-mounted
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

#define DEBUGXXX
// Invoked when received report from device via interrupt endpoint

static uint8_t prev_report[256];
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  if (memcmp(prev_report, report, len))
  {
    for (int i = 0 ; i < len ; i++)
    {
      printf("%02X ", report[i]);
      if ((i % 16) == 15) printf("\r\n");
    }
    printf("\r\n");
  
    for (int i = 0 ; i < len ; i++)
    {
      if(report[i] != prev_report[i]) printf("%d = %02X:%02X ", i, report[i], prev_report[i]);
    }
    printf("\r\n");
    memcpy(prev_report, report, len);

  }

  sleep_ms(2000);
#if 0
#ifdef DEBUGXXX
  printf("hid_report\n");
#endif
  for (int i = 0 ; i < 2 ; i++)
  {
    if (joysticks[i].connected &&
      joysticks[i].dev_addr == dev_addr && 
      joysticks[i].instance == instance)

      process_joy_input(i, report, len);
  }
  #ifdef DEBUGXXX
  printf("tuh_hid_receive_report(%d, %d)\r\n", dev_addr, instance);
#endif
#endif
      if ( !tuh_hid_receive_report(dev_addr, instance) )
      {
        printf("Error: cannot request to receive report\r\n");
      }

}

void schedule_joy_input()
{
  if (joy_mounted)
  {
    static  uint8_t cmd_buf[] = { 0x42, 0x0c, 0x00, 0x00 };
    printf("SENDING PS3 REPoRT\r\n");
    
    sleep_ms(1000);

    tuh_hid_set_report(joysticks[0].dev_addr, joysticks[0].instance, 0xF4, HID_REPORT_TYPE_FEATURE, cmd_buf, sizeof(cmd_buf));
/* 
static    uint8_t led_buf[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x02, 0xff, 0x27, 0x10, 0x00, 0x32, 0xff, 
                                     0x27, 0x10, 0x00, 0x32, 0xff, 0x27, 0x10, 0x00, 
                                     0x32, 0xff, 0x27, 0x10, 0x00, 0x32, 0x00, 0x00, 
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
*/
  printf ("LED1\n");
//  tuh_hid_set_report(joysticks[0].dev_addr, joysticks[0].instance, 0x01, HID_REPORT_TYPE_OUTPUT, led_buf, sizeof(led_buf));

  printf ("FEATURE\n");
//  tuh_hid_set_report(joysticks[0].dev_addr, joysticks[0].instance, 0xF4, HID_REPORT_TYPE_FEATURE, cmd_buf, 4);
  
    joy_mounted = false;
  }
}
  

void tuh_hid_set_report_complete_cb(uint8_t dev_addr, uint8_t instance, uint8_t report_id, 
      uint8_t report_type, uint16_t len)
{
  printf("tuh_hid_set_report_complete_cb\n");
  switch(report_id)
  {
    case 0xF4:
      printf("tuh_hid_receive_report\r\n");
      while (!tuh_hid_receive_report(dev_addr, instance))
      {
        printf("Error: cannot request to receive report\r\n");
        tuh_task();
      }
      break;
  }



#if 0
  // continue to request to receive report
  for (int i = 0 ; i < 2 ; i++)
  {
    if (joysticks[i].connected)
    {
#ifdef DEBUGXXX
  printf("tuh_hid_receive_report(%d, %d)\r\n", joysticks[i].dev_addr, joysticks[i].instance);
#endif
      if ( !tuh_hid_receive_report(joysticks[i].dev_addr, joysticks[i].instance) )
      {
        printf("Error: cannot request to receive report\r\n");
      }
    }
  }
#endif
}

uint8_t get_joy_value_x (uint8_t value)
{
  switch(value)
  {
    case 127:
      return 0;
    case 0:
      return -127;
    case 255:
      return 127;
  }
}


uint8_t get_joy_value_y (uint8_t value)
{
  switch(value)
  {
    case 127:
      return 0;
    case 0:
      return 127;
    case 255:
      return -127;
  }
}

void process_joy_input(uint8_t joynum, const uint8_t* report, uint8_t len)
{
  joystick_report* joy_rpt = (joystick_report *) report;

  for (int i = 0 ; i < len ; i++)
  {
    printf("%02X ", report[i]);
  }
  printf("\r\n");
  if (memcmp(joy_rpt, &joysticks[joynum].prev_report, sizeof(joystick_report)))
  {
    memcpy(&joysticks[joynum].prev_report, joy_rpt, sizeof(joystick_report));

    uint8_t daz_msg[3];
    daz_msg[0] = (joynum == 0) ? DAZ_JOY1 : DAZ_JOY2;
    if (!joy_rpt->btn1) daz_msg[0] |= 1;
    if (!joy_rpt->btn2) daz_msg[0] |= 2;
    if (!joy_rpt->btn3) daz_msg[0] |= 4;
    if (!joy_rpt->btn4) daz_msg[0] |= 8;
    daz_msg[1] = get_joy_value_x(joy_rpt->x);
    daz_msg[2] = get_joy_value_y(joy_rpt->y);
    
 //   printf ("Sending joystick report\r\n");
 #ifndef DEBUGXXX
    printf("Joy = %d, X = %d, Y = %d, btn = %x msg[0] = %02x\r\n", joynum, (int8_t) daz_msg[1], (int8_t) daz_msg[2], daz_msg[0] & 0x0F, daz_msg[0]);
 #endif
    usb_send_byte(daz_msg, 3);
  }

}