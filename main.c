/* USB Host includes */

#include "bsp/board.h"
#include "tusb.h"

#include "pico.h"
#include "pico/stdlib.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"

#include "usb_joystick.h"
#include "daz_audio.h"

/* TODOS:
 */

#include <string.h>
#include <stdio.h>

/* 
 * Dazzler resolutions are:
 * 128x128 (mono), 64x64 (2k mode) and 32x32 (512 byte mode)
 * We choose a 1024x768 display resolution as it is a multiple of 128 x 128
 */
#define WIDTH   128
#define HEIGHT  128
#define NUMCLR  16

#define JOY_POLL_MS   16 /* Poll the joysticks ~ 60 times per second */

//#define DEBUG 
//#define DEBUG0

/* Dazzler packet types */
#define DAZ_MEMBYTE   0x10
#define DAZ_FULLFRAME 0x20
#define DAZ_CTRL      0x30
#define DAZ_CTRLPIC   0x40
#define DAZ_DAC       0x50
#define DAZ_VERSION   0xF0

#define DAZ_JOY1      0x10
#define DAZ_JOY2      0x20
#define DAZ_KEY       0x30
#define DAZ_VSYNC     0x40

#define FEAT_VIDEO    0x01
#define FEAT_JOYSTICK 0x02
#define FEAT_DUAL_BUF 0x04
#define FEAT_VSYNC    0x08
#define FEAT_DAC      0x10
#define FEAT_FRAMEBUF 0x40

#define DAZZLER_VERSION 0x02


void set_vram(int addr, uint8_t value, bool refresh);
void refresh_vram(void);

/*
 * Dazzler control register:
 * D7: on/off
 * D6-D0: screen memory location (not used in client)
 */ 
uint8_t dazzler_ctrl = 0x00;
/* 
 * Dazzler picture control register:
 * D7: not used
 * D6: 1=resolution x4, 0=normal resolution
 * D5: 1=2k memory, 0=512byte memory
 * D4: 1=color, 0=b/w
 * D3-D0: foreground color for x4 high res mode
 */
uint8_t dazzler_picture_ctrl = 0x00;    /* normal res, 512 byte, b/w */
/*
 * The current video mode, set by DAZ_CTRLPIC
 */
enum { mode_32x32c, mode_64x64m, mode_64x64c, mode_128x128m } video_mode = mode_64x64m;

/* Bitmasks for dazzler_picture_ctrl */
#define DPC_RESOLUTION  0x40
#define DPC_MEMORY      0x20
#define DPC_COLOUR      0x10
#define DPC_FOREGROUND  0x0F

/* 16 colours used in colour mode */
uint16_t    colours[NUMCLR] =
{
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u,   0u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(128u,   0u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u, 128u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(128u, 128u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u,   0u, 128u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(128u,   0u, 128u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u, 128u, 128u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(128u, 128u, 128u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u,   0u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u,   0u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u, 255u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u, 255u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u,   0u, 255u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u,   0u, 255u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u, 255u, 255u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u, 255u, 255u)
};

/* 16 colours used in black and white mode */
uint16_t    greys[NUMCLR] =
{
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u,   0u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8( 17u,  17u,  17u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8( 34u,  34u,  34u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8( 51u,  51u,   51u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8( 68u,  68u,  68u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8( 85u,  85u,  85u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(102u, 102u, 102u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(119u, 119u, 119u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(136u, 136u, 136u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(153u, 153u, 153u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(170u, 170u, 170u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(187u, 187u, 187u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(204u, 204u, 204u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(221u, 221u, 221u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(238u, 238u, 238u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u, 255u, 255u)
};

/* Points to the colours table in colour modes and the greys table in B&W modes */
uint16_t *clr_table = greys;

/*
 * Create a custom scanvideo mode that is 128x128 scaled to 
 * 1024 x 768.
 */
extern const scanvideo_timing_t vga_timing_1024x768_60_default;
const scanvideo_mode_t vga_mode_128x128 =
{
    .default_timing = &vga_timing_1024x768_60_default,
    .pio_program = &video_24mhz_composable,
    .width = 1024,
    .height = 768,
    .xscale = 8,
    .yscale = 6,
};

/* 
 * Framebuffer with 16 bits per pixel as the scanline vga library uses
 * 16 bits per pixel colour.
 */
uint16_t frame_buffer[WIDTH * HEIGHT];

/* 
 * This is a copy of the Altair's video ram. We need a copy of this because:
 * 1) When applications change modes, we need to refresh the scanvideo frame buffer from the altair RAM
 * 2) The Altair-duino dazzler code oftens sends the video rame bytes before the video mode, and need to
 *    refresh the scanvideo framebuffer whenever the video mode changes.
 */
uint8_t raw_frame[2048];

/*************************************************************
 * USB Serial port handling                                  *
 *************************************************************/

/* USB receive ring buffer */
#define USB_BUFFER_SIZE 4096
uint8_t usb_buffer[USB_BUFFER_SIZE];
uint8_t *usb_wr = usb_buffer;
uint8_t *usb_rd = usb_buffer;

/* Return true if bytes available to be read */
bool usb_avail()
{
    return usb_rd != usb_wr;
}

/* Return top byte from usb buffer, or 0 if no bytes available */
uint8_t usb_getbyte()
{
    uint8_t result = 0;
    if (usb_avail())
    {
        usb_rd++;
    	if (usb_rd >= (usb_buffer + USB_BUFFER_SIZE))
    	{
            usb_rd = usb_buffer;
    	}
        result = *usb_rd;
    }
    return result;
}

/* Return the top byte from usb buffer. Block until data is available */
uint8_t usb_getbyte_blocking()
{
    while(!usb_avail()) { tuh_task(); }
    return usb_getbyte();
}

/* set a byte into usb buffer */
void usb_setbyte(uint8_t byte)
{
    uint8_t* wr_pos = usb_wr + 1;
    if (wr_pos >= (usb_buffer + USB_BUFFER_SIZE))
    {
#ifdef DEBUG0
        printf ("USB BUFFER WRAPPED\r\n");
#endif
        wr_pos = usb_buffer;
    }
    *wr_pos = byte;
    usb_wr = wr_pos;
}

/* Send buffer via usb serial port */
void usb_send_bytes(uint8_t *buf, int count)
{
    /* Assumes one CDC interface */
    if (tuh_cdc_mounted(0) && count)
    {
#ifdef DEBUG
        printf("usb_write: %d, %d\r\n", idx, count);
#endif
        tuh_cdc_write(0, buf, count);
        tuh_cdc_write_flush(0);
    }
}

/*************************************************************
 * USB Serial routines                                       *
 *************************************************************/

/* Callback when data available on USB */
/* Some weirdness here as FULL_SPEED devices should only send 64 bytes at a time,
 * but the Duo sends 512 byte packets and we need to receive the full 512 to make tinyusb happy */
void tuh_cdc_rx_cb(uint8_t idx)
{
    static uint8_t buf[512];
#ifdef DEBUG
    //printf("*");
#endif
    uint32_t count = tuh_cdc_read(idx, buf, sizeof(buf));
#ifndef PERF
    for (int i = 0 ; i < count ; i++)
    {
        usb_setbyte(buf[i]);
    }
#endif
}

/* Callback when USB Serial device is connected */
void tuh_cdc_mount_cb(uint8_t idx)
{
  tuh_cdc_itf_info_t itf_info = { 0 };
  tuh_cdc_itf_get_info(idx, &itf_info);

  printf("CDC Interface is mounted: address = %u, itf_num = %u\r\n", itf_info.daddr, itf_info.bInterfaceNumber);

#ifdef CFG_TUH_CDC_LINE_CODING_ON_ENUM
  // CFG_TUH_CDC_LINE_CODING_ON_ENUM must be defined for line coding is set by tinyusb in enumeration
  cdc_line_coding_t line_coding = { 0 };
  if ( tuh_cdc_get_local_line_coding(idx, &line_coding) )
  {
    printf("  Baudrate: %lu, Stop Bits : %u\r\n", line_coding.bit_rate, line_coding.stop_bits);
    printf("  Parity  : %u, Data Width: %u\r\n", line_coding.parity  , line_coding.data_bits);
  }
#endif
}

/* Callback wheb USB Serial device is unmounted */
void tuh_cdc_umount_cb(uint8_t idx)
{
  tuh_cdc_itf_info_t itf_info = { 0 };
  tuh_cdc_itf_get_info(idx, &itf_info);

  printf("CDC Interface is unmounted: address = %u, itf_num = %u\r\n", itf_info.daddr, itf_info.bInterfaceNumber);
}

/* Callback when a USB device is mounted */
void tuh_mount_cb(uint8_t dev_addr)
{
#if 0
    int result = tuh_hid_receive_report(dev_addr, 0);
#ifndef DEBUG
            printf("tuh_hid_receive_report(%d, %d) = %d\n", dev_addr, 0, result);
#endif
#endif
  printf("A device with address %d is mounted\r\n", dev_addr);
}

/* Callback when a USB device is unmounted */
void tuh_umount_cb(uint8_t dev_addr)
{
  printf("A device with address %d is unmounted \r\n", dev_addr);
}


/*************************************************************
 * Video Rendering Routines                                  *
 *************************************************************/

/*
 * Renders a line of video. (Runs as dedicated loop on second core)
 * 
 * Uses the COMPOSABLE_RAW_RUN rendering method, which has the byte format
 * COMPOSABLE_RAW_RUN | colour0 | num 32 bit words | colour1 .... colour n | COMPOSABLE_EOL_ALIGN
 * Colours from the line being processed are copied from the frame_buffer into the scan line
 */
volatile bool send_vsync = false;
void __time_critical_func(render_loop) (void)
{
#ifdef DEBUG
    printf ("Starting render\n");
#endif
    absolute_time_t abs_time = get_absolute_time();
    while(true)
    {
        /* Wait for ready to render next scanline */
        struct scanvideo_scanline_buffer *buffer = scanvideo_begin_scanline_generation(true);
        int scanline = scanvideo_scanline_number (buffer->scanline_id);
        uint32_t *vga_buf = buffer->data + 1;
        uint16_t *frame_buf = &frame_buffer[scanline * WIDTH];

        /* Copy colours from framebuffer into scanline buffer */
        memcpy(vga_buf, frame_buf, WIDTH * 2);
        /* TODO: can just make this vga_buf[WIDTH/2] */
        vga_buf += WIDTH/2;
        *vga_buf = COMPOSABLE_EOL_ALIGN << 16;

        /* Set the "header" bytes for the scanline */
        vga_buf = buffer->data;
        vga_buf[0] = (vga_buf[1] << 16) | COMPOSABLE_RAW_RUN;
        vga_buf[1] = (vga_buf[1] & 0xFFFF0000) | (WIDTH - 2);
        buffer->data_used = (WIDTH + 4) / 2;

        /* render video scanline */
        scanvideo_end_scanline_generation (buffer);
    }
}

/* Handle VSYNC */
const uint VSYNC_PIN = PICO_SCANVIDEO_COLOR_PIN_BASE + PICO_SCANVIDEO_COLOR_PIN_COUNT + 1;
void vga_irq_handler() {
    int vsync_current_level = gpio_get(VSYNC_PIN);

    // Note v_sync_polarity == 1 means active-low
    if (vsync_current_level != scanvideo_get_mode().default_timing->v_sync_polarity) 
    {
        send_vsync = true;
    }
    gpio_acknowledge_irq(VSYNC_PIN, vsync_current_level ? GPIO_IRQ_EDGE_RISE : GPIO_IRQ_EDGE_FALL);
}


/* Set up the video mode 
 * 1024x768 mode requires system clock to be set at 130MHz
 * which is done at start of main()
 */
void setup_video(void)
{
    scanvideo_setup(&vga_mode_128x128);
    scanvideo_timing_enable(true);
    gpio_set_irq_enabled(VSYNC_PIN, GPIO_IRQ_EDGE_FALL, true);
    irq_set_exclusive_handler(IO_IRQ_BANK0, vga_irq_handler);
    irq_set_enabled(IO_IRQ_BANK0, true);

#ifdef DEBUG
    printf ("System clock speed %d kHz\n", clock_get_hz (clk_sys) / 1000);
#endif
}


/*************************************************************
 * Framebuffer manipulation routines                         *
 *************************************************************/

/*
 * The following routines set a pixel into the frame_buffer at different resolutions.
 * AThe framebuffer is kept at a fixed 128x128 resolutions, and lower resolutions set
 * multiple pixels for this resolution.
 */
void set_pixel_128(int x, int y, int colour)
{
    colour &= 0x0F;
    if (x >= 0 && x < WIDTH && y >=0 && y < HEIGHT)
    {
        frame_buffer[y * WIDTH + x] = clr_table[colour];
    }
}

void set_pixel_64(int x, int y, int colour)
{
#ifdef DEBUG2
    printf("x = %d, y = %d, Colour = %x, %x\n", x, y, colour, clr_table[colour]);
#endif

    x = x * 2;
    y = y * 2;
    colour &= 0x0F;
    int start = y * WIDTH + x;
    if (x >= 0 && x < WIDTH && y >=0 && y < HEIGHT)
    {
        frame_buffer[start] = clr_table[colour];
        frame_buffer[start + 1] = clr_table[colour];
        start += WIDTH;
        frame_buffer[start] = clr_table[colour];
        frame_buffer[start + 1] = clr_table[colour];
    }
}

void set_pixel_32(int x, int y, int colour)
{
    x = x * 4;
    y = y * 4;
    colour &= 0x0F;
    int start = y * WIDTH + x;
    if (x >= 0 && x < WIDTH && y >=0 && y < HEIGHT)
    {
        frame_buffer[start] = clr_table[colour];
        frame_buffer[start + 1] = clr_table[colour];
        frame_buffer[start + 2] = clr_table[colour];
        frame_buffer[start + 3] = clr_table[colour];
        start += WIDTH;
        frame_buffer[start] = clr_table[colour];
        frame_buffer[start + 1] = clr_table[colour];
        frame_buffer[start + 2] = clr_table[colour];
        frame_buffer[start + 3] = clr_table[colour];
        start += WIDTH;
        frame_buffer[start] = clr_table[colour];
        frame_buffer[start + 1] = clr_table[colour];
        frame_buffer[start + 2] = clr_table[colour];
        frame_buffer[start + 3] = clr_table[colour];
        start += WIDTH;
        frame_buffer[start] = clr_table[colour];
        frame_buffer[start + 1] = clr_table[colour];
        frame_buffer[start + 2] = clr_table[colour];
        frame_buffer[start + 3] = clr_table[colour];
    }
}


/* Set a pixel into frame_buffer and raw_frame 
 * If called with refresh=true, only set frame_buffer
 * refresh mode is used when video mode changes to set the
 * frame buffer from the current raw_frame contents */
void set_vram(int addr, uint8_t value, bool refresh)
{
#ifdef PERF
    return;
#endif
#ifdef DEBUG2
    printf ("set_vram(%d,%x, %d)\n", addr, value, refresh);
#endif
    /* raw_frame stores a copy of the Dazzler video ram. 
     * When setting ram values, copy into the raw_frame and vga framebuffer
     * refresh is set when refreshing vga framebuffer from raw_frame. So don't need to set
     * raw_frame in that case
     */
    if (!refresh)
        raw_frame[addr] = value;

    int x, y;

    /* For monochrome modes, foreground colour comes from the picture control message */
    uint8_t mono_clr = dazzler_picture_ctrl & 0x0F;
    switch(video_mode)
    {
        case mode_128x128m:
        {
            /* In this mode each byte contains 8 individual on/off pixels*/
            /* this also needs to do the different addressing for each quater of the screen */
            if (addr < 512)             /* First quadrant */
            {
                y = addr / 16 * 2;
                x = (addr * 4) % 64;
           } 
            else if (addr < 1024)
            {
                y = (addr - 512) / 16 * 2;
                x = (((addr - 512) * 4) % 64) + 64;
            }
            else if (addr < 1536)
            {
                y = (addr - 1024) / 16 * 2 + 64;
                x = ((addr - 1024) * 4) % 64;
            }
            else
            {
                y = (addr - 1536) / 16 * 2 + 64;
                x = (((addr - 1536) * 4) % 64) + 64;
            }

            /* If bit is set, set pixel to foreground colour, otherwise set to black */
            /* Pixel layout is as follows (wher e D0 is bit 0):
             * | D0 | D1 | D4 | D5 |
             * | D2 | D3 | D6 | D7 | 
             */
            set_pixel_128(x, y, (value & 0x01) ? mono_clr : 0);
            set_pixel_128(x + 1, y, (value & 0x02) ? mono_clr : 0);
            set_pixel_128(x, y + 1, (value & 0x04) ? mono_clr : 0);
            set_pixel_128(x + 1, y + 1, (value & 0x08) ? mono_clr : 0);
            set_pixel_128(x + 2, y, (value & 0x10) ? mono_clr : 0);
            set_pixel_128(x + 3, y, (value & 0x20) ? mono_clr : 0);
            set_pixel_128(x + 2, y + 1, (value & 0x40) ? mono_clr : 0);
            set_pixel_128(x + 3, y + 1, (value & 0x80) ? mono_clr : 0);
            break;
        }
        case mode_64x64m:
        {
            y = addr / 16 * 2;
            x = (addr * 4) % 64;

            /* If bit is set, set pixel to foreground colour, otherwise set to black */
            /* Pixel layout is the same as 128x128m mode */
            set_pixel_64(x, y, (value & 0x01) ? mono_clr : 0);
            set_pixel_64(x + 1, y, (value & 0x02) ? mono_clr : 0);
            set_pixel_64(x, y + 1, (value & 0x04) ? mono_clr : 0);
            set_pixel_64(x + 1, y + 1, (value & 0x08) ? mono_clr : 0);
            set_pixel_64(x + 2, y, (value & 0x10) ? mono_clr : 0);
            set_pixel_64(x + 3, y, (value & 0x20) ? mono_clr : 0);
            set_pixel_64(x + 2, y + 1, (value & 0x40) ? mono_clr : 0);
            set_pixel_64(x + 3, y + 1, (value & 0x80) ? mono_clr : 0);
            break;        
        }
        case mode_64x64c:
        {
            if (addr < 512) /* First quadrant */
            {
                y = addr * 2 / 32;
                x = addr * 2 % 32;
            } 
            else if (addr < 1024)
            {
                y = (addr - 512) * 2 / 32;
                x = ((addr - 512) * 2 % 32) + 32;
            }
            else if (addr < 1536)
            {
                y = ((addr - 1024) * 2 / 32) + 32;
                x = (addr - 1024) * 2 % 32;
            }
            else
            {
                y = ((addr - 1536) * 2 / 32) + 32;
                x = ((addr - 1536) * 2 % 32) + 32;
            }
            /* Each byte contains 2 pixels */
            set_pixel_64(x + 0, y, value & 0x0F);
            set_pixel_64(x + 1, y, value >> 4);
            break;
        }
        case mode_32x32c:
        {
            int y = addr * 2 / 32;
            int x = (addr * 2) % 32;
            /* Each byte contains 2 pixels */
            set_pixel_32(x + 0, y, value & 0x0F);
            set_pixel_32(x + 1, y, value >> 4);
            break;
        }
    }
}

/* Used when chaning video modes to copy from raw_frame into frame_buffer with the new mode */
void refresh_vram()
{
    for (int i = 0 ; i < 2048 ; i++)
    {
        set_vram(i, raw_frame[i], false);
    }
}


/*************************************************************
 * Main processing loop for USB                              *
 *************************************************************/

/*
 * Process commands coming from the Altair-duino via the USB serial interface
 */
void process_usb_commands()
{
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    int led_status = 0;

    absolute_time_t abs_time = get_absolute_time();
    /* poll joystick ~60 times per second */
    absolute_time_t joy_poll_time = make_timeout_time_ms(JOY_POLL_MS);

    uint8_t c = 0;
    while (true)
    {
        abs_time = get_absolute_time();

        /*
         * If nothing available on USB, then schedule request to poll joysticks
         * and service USB tasks.
         */
        if (!usb_avail())
        {
            if (send_vsync)
            {
                static uint8_t vsync = DAZ_VSYNC;
                usb_send_bytes(&vsync, 1);
                send_vsync = false;
            }
            if (absolute_time_diff_us(joy_poll_time, abs_time) >= 0)
            {
                schedule_joy_input();
                joy_poll_time = make_timeout_time_ms(JOY_POLL_MS);
            }
       	    tuh_task();
            continue;
        }
        /* Otherwise service the USB serial */
        c = usb_getbyte();
   
        /* TODO: Implement LED flashing algo */
//        led_status = (to_ms_since_boot(abs_time) / 50) % 2;
//        gpio_put(LED_PIN, led_status);

        switch (c & 0xF0)
        {
            case DAZ_VERSION:
            {
#ifdef DEBUG
    printf("VERSION\r\n");
#endif
                static uint8_t buf[3];
                buf[0] = DAZ_VERSION | (DAZZLER_VERSION & 0x0F);
                buf[1] = FEAT_VIDEO | FEAT_FRAMEBUF | FEAT_JOYSTICK | FEAT_DAC | FEAT_VSYNC;
                buf[2] = 0;
                usb_send_bytes(buf, 3);
#ifdef PERF
    while(1) tuh_task();
#endif

                break;
            }
            case DAZ_CTRL:
            {
#ifdef DEBUG
    printf("DAZ_CTRL\r\n");
#endif
                if ((c & 0x0F) == 0)
                {
#ifdef DEBUG
    printf("DAZ_CTRL 1\r\n");
#endif
                    uint8_t prev_dazzler_ctrl = dazzler_ctrl;
                    dazzler_ctrl = (uint8_t) usb_getbyte_blocking();
                    if (dazzler_ctrl != prev_dazzler_ctrl)
                    {
#ifdef DEBUG
    printf("DAZ_CTRL ON\r\n");
#endif

                        /* If Dazzler is turned on */
                        if (dazzler_ctrl & 0x80)
                        {
#ifdef DEBUG
    printf("DAZ_CTRL REFRESH\r\n");
#endif
                            refresh_vram();
#ifdef DEBUG
    printf("DAZ_CTRL REFRESH DONE\r\n");
#endif
                        }
                        else /* Dazzler turned off */
                        {
#ifdef DEBUG
    printf("DAZ_CTRL OFF\r\n");
#endif
                            /* blank screen */
                            memset(frame_buffer, 0, sizeof(frame_buffer));
                        }
                    }
                }
#ifdef DEBUG
    printf("DAZ_CTRL END\r\n");
#endif
                break;
            }
            case DAZ_CTRLPIC:
            {
#ifdef DEBUG
    printf("DAZ_CTRLPIC\r\n");
#endif
                uint8_t prev_picture_ctrl = dazzler_picture_ctrl;

                if ((c & 0x0F) == 0)
                {
                    dazzler_picture_ctrl = (uint8_t) usb_getbyte_blocking();

                    /* If the picture control changed, set the video mode and colour palette */
                    if (prev_picture_ctrl != dazzler_picture_ctrl)
                    {
                        if (dazzler_picture_ctrl & DPC_RESOLUTION)  /* X4 mode */
                            if (dazzler_picture_ctrl & DPC_MEMORY)  /* 2048 byte mode*/
                                video_mode = mode_128x128m;
                            else                                    /* 512 byte mode */
                                video_mode = mode_64x64m;
                        else                                        /* Normal mode */
                            if (dazzler_picture_ctrl & DPC_MEMORY)  /* 2048 byte mode*/
                                video_mode = mode_64x64c;
                            else                                    /* 512 byte mode */
                                video_mode = mode_32x32c;

                        clr_table = (dazzler_picture_ctrl & DPC_COLOUR) ? colours : greys;
    
                        refresh_vram();
                    }
                }
#ifdef DEBUG
    printf("DAZ_CTRLPIC END\r\n");
#endif
                break;
            }
            case DAZ_MEMBYTE:
            {
#ifdef DEBUG
    printf("DAZ_MEMBYTE\r\n");
#endif

                int addr = (c & 0x0F) * 256 + (uint8_t) usb_getbyte_blocking();
                uint8_t value = (uint8_t) usb_getbyte_blocking();
                set_vram(addr, value, false);
                break;
            }
            case DAZ_FULLFRAME:
            {
#ifdef DEBUG
    printf("DAZ_FULLFRAME\r\n");
#endif
                if((c & 0x06) == 0)
                {
#ifdef DEBUG
    printf("DAZ_FULLFRAME 1\r\n");
#endif
                    int addr = (c & 0x08) * 256;
                    int count = (c & 0x01) ? 2048 : 512;
#ifdef DEBUG
    printf("DAZ_FULLFRAME %d, %d\r\n", addr, count);
#endif
                    for (int i = 0 ; i < count ; i++)
                    {
#ifdef DEBUG
    printf("DAZ_FULLFRAME %d\r\n", i);
#endif
                        uint8_t value = (uint8_t) usb_getbyte_blocking();
                        set_vram(addr + i, value, false);
                    }
                }
#ifdef DEBUG
    printf("DAZ_FULLFRAME_END\r\n");
#endif
                break;
            }
            case DAZ_DAC:
            {
                uint8_t channel = (c & 0x0f) == 0 ? 0: 1;
                uint16_t delay_us = usb_getbyte_blocking() | (usb_getbyte_blocking() << 8); // Endian check?
                uint8_t sample = usb_getbyte_blocking();
                audio_add_sample(channel, delay_us, sample); 
#ifdef DEBUG
                printf("DAC: %d, %d, %x\n", channel, delay_us, sample);
#endif
                break;
            }
        }
    }
}

/* Start running video on core 1*/
void core1_main()
{
    setup_video();
    render_loop();
}


int main(void)
{
    /* 1024x768 mode requires a system clock of 130MHz */
 
    set_sys_clock_khz(130000, true);

    board_init();
    stdio_init_all();
    tuh_init(BOARD_TUH_RHPORT);
    audio_init();
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    int led_status = 1;

    gpio_put(LED_PIN, led_status);
    memset(frame_buffer, 0, sizeof(frame_buffer));
    int clr = 0;
    sleep_ms(1000);
    
    multicore_launch_core1(core1_main);
#ifdef DEBUG
    printf("processing serial commands\n");
#endif
#ifdef AUDIO_TEST
    int timeout = 2336;
    absolute_time_t sample_time = make_timeout_time_us(timeout);
    int samples[2] = { 0x7f, 0x81 };
    int sample_nr = 0;
    while (true) {
        for (int i = 0 ; i < 500 ; i++)
        {
            absolute_time_t abs_time = get_absolute_time();        
            if(absolute_time_diff_us(sample_time, abs_time) >= 0)
            {
                audio_add_sample(0, 50, samples[sample_nr]);
                sample_nr ^= 1;
                sample_time = make_timeout_time_us(timeout);
            }
        }
    }
#endif

    printf("READY\n");
    process_usb_commands();
}

