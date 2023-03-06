
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


/*****************************************************************************
 * PICO DAZZLER
 *
 * Cromemco Dazzler emulation using Raspberry Pi Pico
 * 
 *****************************************************************************/

#include "bsp/board.h"
#include "tusb.h"

#include "pico.h"
#include "pico/stdlib.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"

#include "hid_devices.h"
#include "daz_audio.h"

#include <string.h>
#include <stdio.h>

#define DEBUG_INFO  DEBUG_MAIN
#define DEBUG_TRACE TRACE_MAIN
#include "debug.h"

/* 
 * Dazzler resolutions are:
 * 128x128 (mono), 64x64 (2k mode) and 32x32 (512 byte mode)
 * We choose a 1024x768 display resolution as it is a multiple of 128 x 128
 */
#define WIDTH   128
#define HEIGHT  128
#define NUMCLR  16

#define HID_POLL_MS   10    /* Poll HID devices for input @ 100 times per second */

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
#define FEAT_KEYBOARD 0x20
#define FEAT_FRAMEBUF 0x40
#define DAZZLER_VERSION 0x02


void set_vram(int buffer_nr, int addr, uint8_t value, bool refresh);
void refresh_vram(int buffer_nr);

/*
 * Dazzler control register:
 * D7: on/off
 * D6-D0: screen memory location (not used in client)
 * D0: Selects the active frame buffer.
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
enum vid_mode { mode_32x32c, mode_64x64m, mode_64x64c, mode_128x128m } video_mode = mode_64x64m;

/* Bitmasks for dazzler_picture_ctrl */
#define DPC_RESOLUTION  0x40
#define DPC_MEMORY      0x20
#define DPC_COLOUR      0x10
#define DPC_FOREGROUND  0x0F

/* Bitmasks for DAZ_CTRL */
#define DC_ON           0x80

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
    .width = 128,
    .height = 128,
    .xscale = 8,
    .yscale = 6,
};

/* The Altair Duino supports 2 frame buffers for optimisation
 * Some programs quickly cycle between two vram addresses and 
 * having 2 frame buffers saves having to resend a full frame 
 * of data on each cycle.
 */
/* The frame_buffer to generate VGA from (0 or 1)*/
uint active_frame_buffer = 0;

/* 
 * Framebuffers with 16 bits per pixel as the scanline vga library uses
 * 16 bits per pixel colour.
 * First 2 framebuffers are for dual buffering. The 3rd is a blank framebuffer
 * for when the Dazzler is turned off.
 */
uint16_t frame_buffers[3][WIDTH * HEIGHT];

/* 
 * This is a copy of the Altair's video ram. We need a copy of this because:
 * 1) When applications change modes, we need to refresh the scanvideo frame buffer from the altair RAM
 * 2) The Altair-duino dazzler code oftens sends the video ram bytes before the video mode, and need to
 *    refresh the scanvideo framebuffer whenever the video mode changes.
 */
uint8_t raw_frames[2][2048];

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

uint8_t usb_peekbyte()
{
    uint8_t result = 0;
    uint8_t *tmp_rd = usb_rd + 1;
    if (usb_avail())
    {
    	if (tmp_rd >= (usb_buffer + USB_BUFFER_SIZE))
    	{
            tmp_rd = usb_buffer;
    	}
        result = *tmp_rd;
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
        PRINT_INFO("USB BUFFER WRAPPED\n");
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

    uint32_t count = tuh_cdc_read(idx, buf, sizeof(buf));

    for (int i = 0 ; i < count ; i++)
    {
        usb_setbyte(buf[i]);
    }
}

/* Callback when USB Serial device is connected */
void tuh_cdc_mount_cb(uint8_t idx)
{
    tuh_cdc_itf_info_t itf_info = { 0 };
    tuh_cdc_itf_get_info(idx, &itf_info);
    
    printf("CDC Interface is mounted: address = %u, itf_num = %u\n", itf_info.daddr, itf_info.bInterfaceNumber);

#ifdef CFG_TUH_CDC_LINE_CODING_ON_ENUM
    /* CFG_TUH_CDC_LINE_CODING_ON_ENUM must be defined to set correct Baud Rate / Stop bits and Parity */
    cdc_line_coding_t line_coding = { 0 };
    if ( tuh_cdc_get_local_line_coding(idx, &line_coding) )
    {
        printf("  Baudrate: %lu, Stop Bits : %u\n", line_coding.bit_rate, line_coding.stop_bits);
        printf("  Parity  : %u, Data Width: %u\n", line_coding.parity  , line_coding.data_bits);
    }
#endif
}

/* Callback wheb USB Serial device is unmounted */
void tuh_cdc_umount_cb(uint8_t idx)
{
    tuh_cdc_itf_info_t itf_info = { 0 };
    tuh_cdc_itf_get_info(idx, &itf_info);

    printf("CDC Interface is unmounted: address = %u, itf_num = %u\n", itf_info.daddr, itf_info.bInterfaceNumber);
}

/* Callback when a USB device is mounted */
void tuh_mount_cb(uint8_t dev_addr)
{
    printf("A device with address %d is mounted\n", dev_addr);
}

/* Callback when a USB device is unmounted */
void tuh_umount_cb(uint8_t dev_addr)
{
    printf("A device with address %d is unmounted \n", dev_addr);
}


/*************************************************************
 * Video Rendering Routines                                  *
 *************************************************************/

/*
 * Renders a line of video. (Runs as dedicated loop on second core)
 * 
 * Uses the COMPOSABLE_RAW_RUN rendering method, which has the word format:
 * COMPOSABLE_RAW_RUN | colour0 | num 32 bit words | colour1 .... colour n | COMPOSABLE_EOL_ALIGN
 * Colours from the line being processed are copied from the frame_buffer into the scan line
 */
volatile bool send_vsync = false;

void __time_critical_func(render_loop) (void)
{
    static int display_frame_buffer = 0;
    PRINT_INFO("Starting render\n");
    while(true)
    {
        /* Wait for ready to render next scanline */
        struct scanvideo_scanline_buffer *buffer = scanvideo_begin_scanline_generation(true);
        int scanline = scanvideo_scanline_number(buffer->scanline_id);
        if (scanline == 0)
        {
            if (dazzler_ctrl & DC_ON)
            {
                /* Draw a full frame before swapping frame buffers */
                display_frame_buffer = active_frame_buffer;
            }
            else
            {
                /* Dazzler is off, display from "bank" framebuffer */
                display_frame_buffer = 2;
            }
        }
        uint32_t *vga_buf = buffer->data + 1;
        uint16_t *frame_buf = &frame_buffers[display_frame_buffer][scanline * WIDTH];

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
        scanvideo_end_scanline_generation(buffer);
    }
}

/* Handle VSYNC */
static const uint VSYNC_PIN = PICO_SCANVIDEO_COLOR_PIN_BASE + PICO_SCANVIDEO_COLOR_PIN_COUNT + 1;
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

    PRINT_INFO("System clock speed %d kHz\n", clock_get_hz (clk_sys) / 1000);
}


/*************************************************************
 * Framebuffer manipulation routines                         *
 *************************************************************/

/* Set active framebuffer to 0/1 for dual buf */
void set_active_framebuffer(int frame_buffer_nr)
{
    PRINT_INFO("Setting Active Framebuffer to %d\n", frame_buffer_nr);
    active_frame_buffer = frame_buffer_nr;
}

/*
 * The following routines set a pixel into the frame_buffer at different resolutions.
 * The framebuffer is kept at a fixed 128x128 resolutions, and lower resolutions set
 * multiple pixels for this resolution.
 */
void __time_critical_func(set_pixel_128)(uint16_t* frame_buffer, int x, int y, int colour)
{
    colour &= 0x0F;
    if (x >= 0 && x < WIDTH && y >=0 && y < HEIGHT)
    {
        frame_buffer[y * WIDTH + x] = clr_table[colour];
    }
}

void __time_critical_func(set_pixel_64)(uint16_t *frame_buffer, int x, int y, int colour)
{
    PRINT_TRACE("x = %d, y = %d, Colour = %x, %x\n", x, y, colour, clr_table[colour]);

    x = x * 2;
    y = y * 2;
    colour &= 0x0F;
    int start = y * WIDTH + x;
    uint16_t clr = clr_table[colour];
    if (x >= 0 && x < WIDTH && y >=0 && y < HEIGHT)
    {
        frame_buffer[start] = clr;
        frame_buffer[start + 1] = clr;
        start += WIDTH;
        frame_buffer[start] = clr;
        frame_buffer[start + 1] = clr;
    }
}

void __time_critical_func(set_pixel_32)(uint16_t *frame_buffer, int x, int y, int colour)
{
    x = x * 4;
    y = y * 4;
    colour &= 0x0F;
    int start = y * WIDTH + x;
    uint16_t clr = clr_table[colour];
    if (x >= 0 && x < WIDTH && y >=0 && y < HEIGHT)
    {

        frame_buffer[start] = clr;
        frame_buffer[start + 1] = clr;
        frame_buffer[start + 2] = clr;
        frame_buffer[start + 3] = clr;
        start += WIDTH;
        frame_buffer[start] = clr;
        frame_buffer[start + 1] = clr;
        frame_buffer[start + 2] = clr;
        frame_buffer[start + 3] = clr;
        start += WIDTH;
        frame_buffer[start] = clr;
        frame_buffer[start + 1] = clr;
        frame_buffer[start + 2] = clr;
        frame_buffer[start + 3] = clr;
        start += WIDTH;
        frame_buffer[start] = clr;
        frame_buffer[start + 1] = clr;
        frame_buffer[start + 2] = clr;
        frame_buffer[start + 3] = clr;
    }
}


/* Set a pixel into frame_buffer and raw_frame 
 * If called with refresh=true, only set frame_buffer
 * refresh mode is used when video mode changes to set the
 * frame buffer from the current raw_frame contents */
void __time_critical_func(set_vram)(int buffer_nr, int addr, uint8_t value, bool refresh)
{
    PRINT_TRACE("set_vram(%d, %d, %x, %d)\n", buffer_nr, addr, value, refresh);
    /* raw_frame stores a copy of the Dazzler video ram. 
     * When setting ram values, copy into both the raw_frame and vga framebuffer
     * refresh is set when refreshing vga framebuffer from raw_frame. So don't need to set
     * raw_frame in that case */
    uint16_t *frame_buffer = frame_buffers[buffer_nr];
    if (!refresh)
    {
        uint8_t *raw_frame = raw_frames[buffer_nr];
        raw_frame[addr] = value;
    }

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
            /* Pixel layout is as follows (where D0 is bit 0):
             * | D0 | D1 | D4 | D5 |
             * | D2 | D3 | D6 | D7 | 
             */
            set_pixel_128(frame_buffer ,x, y, (value & 0x01) ? mono_clr : 0);
            set_pixel_128(frame_buffer ,x + 1, y, (value & 0x02) ? mono_clr : 0);
            set_pixel_128(frame_buffer ,x, y + 1, (value & 0x04) ? mono_clr : 0);
            set_pixel_128(frame_buffer ,x + 1, y + 1, (value & 0x08) ? mono_clr : 0);
            set_pixel_128(frame_buffer ,x + 2, y, (value & 0x10) ? mono_clr : 0);
            set_pixel_128(frame_buffer ,x + 3, y, (value & 0x20) ? mono_clr : 0);
            set_pixel_128(frame_buffer ,x + 2, y + 1, (value & 0x40) ? mono_clr : 0);
            set_pixel_128(frame_buffer ,x + 3, y + 1, (value & 0x80) ? mono_clr : 0);
            break;
        }
        case mode_64x64m:
        {
            y = addr / 16 * 2;
            x = (addr * 4) % 64;

            /* If bit is set, set pixel to foreground colour, otherwise set to black */
            /* Pixel layout is the same as 128x128m mode */
            set_pixel_64(frame_buffer, x, y, (value & 0x01) ? mono_clr : 0);
            set_pixel_64(frame_buffer, x + 1, y, (value & 0x02) ? mono_clr : 0);
            set_pixel_64(frame_buffer, x, y + 1, (value & 0x04) ? mono_clr : 0);
            set_pixel_64(frame_buffer, x + 1, y + 1, (value & 0x08) ? mono_clr : 0);
            set_pixel_64(frame_buffer, x + 2, y, (value & 0x10) ? mono_clr : 0);
            set_pixel_64(frame_buffer, x + 3, y, (value & 0x20) ? mono_clr : 0);
            set_pixel_64(frame_buffer, x + 2, y + 1, (value & 0x40) ? mono_clr : 0);
            set_pixel_64(frame_buffer, x + 3, y + 1, (value & 0x80) ? mono_clr : 0);
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
            set_pixel_64(frame_buffer, x + 0, y, value & 0x0F);
            set_pixel_64(frame_buffer, x + 1, y, value >> 4);
            break;
        }
        case mode_32x32c:
        {
            int y = addr * 2 / 32;
            int x = (addr * 2) % 32;
            /* Each byte contains 2 pixels */
            set_pixel_32(frame_buffer, x + 0, y, value & 0x0F);
            set_pixel_32(frame_buffer, x + 1, y, value >> 4);
            break;
        }
    }
}

/* Used when changing video modes to copy from raw_frame into frame_buffer with the new mode */
void refresh_vram(int buffer_nr)
{
    PRINT_TRACE("Refresh VRAM(%d)\n", buffer_nr);
    uint8_t *raw_frame = raw_frames[buffer_nr];
    for (int i = 0 ; i < 2048 ; i++)
    {
        set_vram(buffer_nr, i, raw_frame[i], false);
    }
}

/*************************************************************
 * Main processing loop for USB                              *
 *************************************************************/

/*
 * Optimisation looks for pairs of CTRL and CTRLPIC in the input stream to
 * reduce the number of refreshses required. Issue is that some apps send CTRL first
 * then CTRLPIC and others in the reverse order. So when the video mode is set, it is 
 * not certain which frame buffer that will be for. To avoid having to refresh the 
 * frame buffer from raw frame buffer each time, we look for pairs and only refresh vram if
 * the mode for a particular buffer has changed. Otherwise if no pair vram refresh has to be 
 * called for both CTRL and CTRLPIC.
 */
int daz_ctrl(uint8_t c)
{
    PRINT_INFO("DAZ_CTRL\n");
    if ((c & 0x0F) == 0)
    {
        uint8_t prev_dazzler_ctrl = dazzler_ctrl;
        dazzler_ctrl = (uint8_t) usb_getbyte_blocking();
        if (dazzler_ctrl != prev_dazzler_ctrl)
        {
            /* If Dazzler is turned on */
            if (dazzler_ctrl & DC_ON)
            {
                PRINT_INFO("DAZ_CTRL ON\n");

                /* Set framebuffer to 1 or 2 */
                PRINT_TRACE("Buffer set to %d\n", dazzler_ctrl &0x01);
                return dazzler_ctrl & 0x01;
            }
            else /* Dazzler turned off */
            {
                /* The VGA rendering routine will blank screen on next frame */
                PRINT_INFO("DAZ_CTRL OFF\n");
            }
        }
    }
    PRINT_TRACE("Buffer set to %d\n", dazzler_ctrl &0x01);
    return active_frame_buffer;
}

bool daz_ctrlpic(uint8_t c)
{
    PRINT_INFO("DAZ_CTRLPIC\n");
    uint8_t prev_picture_ctrl = dazzler_picture_ctrl;
    bool mode_changed = false;

    if ((c & 0x0F) == 0)
    {
        dazzler_picture_ctrl = (uint8_t) usb_getbyte_blocking();

        /* If the picture control changed, set the video mode and colour palette */
        if (prev_picture_ctrl != dazzler_picture_ctrl)
        {
            PRINT_TRACE("DAZ_CTRLPIC: Changing mode\n");
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
            mode_changed = true;
        }
    }
    PRINT_TRACE("Video mode set to: %d\n", video_mode);
    return mode_changed;
}

/*
 * Process commands coming from the Altair-duino via the USB serial interface
 */
void process_usb_commands()
{

    absolute_time_t abs_time = get_absolute_time();
    /* poll joystick ~60 times per second */
    absolute_time_t hid_poll_time = make_timeout_time_ms(HID_POLL_MS);

    uint8_t c = 0;

    PRINT_INFO("processing usb serial commands\n");

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
            if (absolute_time_diff_us(hid_poll_time, abs_time) >= 0)
            {
                hid_schedule_device_poll();
                hid_poll_time = make_timeout_time_ms(HID_POLL_MS);
            }
       	    tuh_task();
            continue;
        }
        /* Otherwise service the USB serial */
        c = usb_getbyte();
   
        switch (c & 0xF0)
        {
            case DAZ_VERSION:
            {
                PRINT_INFO("VERSION\n");
                static uint8_t buf[3];
                buf[0] = DAZ_VERSION | (DAZZLER_VERSION & 0x0F);
                buf[1] = FEAT_VIDEO | FEAT_DUAL_BUF | FEAT_JOYSTICK | FEAT_DAC | FEAT_VSYNC | FEAT_KEYBOARD;
                buf[2] = 0;
                usb_send_bytes(buf, 3);

                break;
            }
            case DAZ_CTRL:
            {
                int fb = daz_ctrl(c);
                if ((usb_peekbyte() & 0xF0) == DAZ_CTRLPIC)
                {
                    PRINT_TRACE("CTRL + CTRLPIC pair\n");
                    /* If video mode changed, then refresh */
                    if (daz_ctrlpic(usb_getbyte()))
                    {
                        refresh_vram(fb);
                    }
                }
                else
                {
                    refresh_vram(fb);
                }
                active_frame_buffer = fb;
                break;
            }
            case DAZ_CTRLPIC:
            {
                int fb = active_frame_buffer;
                bool mode_changed = daz_ctrlpic(c);
                if ((usb_peekbyte() & 0xF0) == DAZ_CTRL)
                {
                    PRINT_TRACE("CTRLPIC + CTRL pair\n");
                    fb = daz_ctrl(usb_getbyte());
                }
                if (mode_changed)
                {
                    refresh_vram(fb);
                }
                active_frame_buffer = fb;                
                break;
            }
            case DAZ_MEMBYTE:
            {
                int buffer_nr = (c & 0x08) ? 1 : 0;
                int addr = (c & 0x07) * 256 + (uint8_t) usb_getbyte_blocking();
                uint8_t value = (uint8_t) usb_getbyte_blocking();
                PRINT_INFO("DAZ_MEMBYTE %02x, %d, %d, %x\n", c, buffer_nr, addr, value);
                set_vram(buffer_nr, addr, value, false);
                break;
            }
            case DAZ_FULLFRAME:
            {
                PRINT_INFO("DAZ_FULLFRAME\n");
                if((c & 0x06) == 0)
                {
                    int buffer_nr  = (c & 0x08) ? 1 : 0;
                    int count = (c & 0x01) ? 2048 : 512;
                    PRINT_INFO("DAZ_FULLFRAME %d, %d\n", buffer_nr, count);
                    for (int i = 0 ; i < count ; i++)
                    {
                        PRINT_TRACE("DAZ_FULLFRAME value %d\n", i);
                        uint8_t value = (uint8_t) usb_getbyte_blocking();
                        set_vram(buffer_nr, i, value, false);
                    }
                }
                break;
            }
            case DAZ_DAC:
            {
                uint8_t channel = (c & 0x0f) == 0 ? 0: 1;
                uint16_t delay_us = usb_getbyte_blocking() | (usb_getbyte_blocking() << 8); // Endian check?
                uint8_t sample = usb_getbyte_blocking();
                audio_add_sample(channel, delay_us, sample); 
                PRINT_INFO("DAC: %d, %d, %x\n", channel, delay_us, sample);
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

    /* Turn on LED to indicate sucessful initialization */
    gpio_put(LED_PIN, 1);
    memset(frame_buffers, 0, sizeof(frame_buffers));

    sleep_ms(1000);
    
    /* Start VGA rendering */
    multicore_launch_core1(core1_main);

    printf("READY\n");
    /* Start accepting Dazzler commands from Altair-Duino */
    process_usb_commands();
}

