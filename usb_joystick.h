
/* This structure based on a USB SNES controller. */
/* Needs to be customised for other controllers */

#include <stdint.h>

typedef struct TU_ATTR_PACKED
{
  uint8_t x, y;
  uint8_t unused1, unused2, unused3;
  struct {
    uint8_t unused4  : 4; // (hat format, 0x08 is released, 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW)
    uint8_t btn1     : 1; // west
    uint8_t btn2     : 1; // south
    uint8_t btn3     : 1; // east
    uint8_t btn4     : 1; // north
  };

  struct {
    uint8_t l1     : 1;
    uint8_t r1     : 1;
    uint8_t l2     : 1; /* unused */
    uint8_t r2     : 1; /* unused */
    uint8_t select : 1;
    uint8_t start  : 1;
    uint8_t l3     : 1; /* unused */
    uint8_t r3     : 1; /* unused */
  };
  
} joystick_report;

/*
typedef struct {
  uint8_t ignored1, ignored2, ignored3;

  struct {
    uint8_t unused4  : 4; // (hat format, 0x08 is released, 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW)
    uint8_t btn1     : 1; // west
    uint8_t btn2     : 1; // south
    uint8_t btn3     : 1; // east
    uint8_t btn4     : 1; // north
  };

}*/

typedef struct 
{
  uint8_t connected;
  uint8_t dev_addr;
  uint8_t instance;
  joystick_report prev_report;
} usb_joystick;

/* Poll usb for input */
void schedule_joy_input();
