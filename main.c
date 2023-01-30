#include "pico.h"
#include "pico/stdlib.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"

/* TODOS:
 * Should check colour / b&w regardless of the mode
 * Need to implement second colour table for b&w mode.
 * Move the graphic mode and B&W calculation to DAZ_CTRLPIC
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

//#define DEBUG

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
 * D4: 1=color, 0=monochrome
 * D3-D0: foreground color for x4 high res mode
 */
#define DPC_RESOLUTION  0x40
#define DPC_MEMORY      0x20
#define DPC_COLOUR      0x10
#define DPC_FOREGROUND  0x0F
uint8_t dazzler_picture_ctrl = 0x10;

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
uint16_t *clr_table = colours;

/*
 * Create a custom video mode that is 128x128 scaled to 
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

/* Framebuffer with 16 bits per pixel as
 * the scanline vga library needs 16 bits per pixel colour
 */
uint16_t frame_buffer[WIDTH * HEIGHT];

/* The "control port" data is being sent after the frame data, event with the FRAMEBUF 
 * attribute being sent. So as a work-around store the raw frame data and redraw the
 * frame every time we get a CTRL_PIC packet.
 */
uint8_t raw_frame[2048];

/*
 * The current video mode, set by DAZ_CTRLPIC
 */
enum { mode_32x32c, mode_64x64m, mode_64x64c, mode_128x128m } video_mode = mode_128x128m;


void __time_critical_func(render_loop) (void)
{
#ifdef DEBUG
    printf ("Starting render\n");
#endif
    while(true)
    {
        struct scanvideo_scanline_buffer *buffer = scanvideo_begin_scanline_generation (true);
        int scanline = scanvideo_scanline_number (buffer->scanline_id);
        uint32_t *vga_buf = buffer->data + 1;
        uint16_t *frame_buf = &frame_buffer[scanline * WIDTH];

        memcpy(vga_buf, frame_buf, WIDTH * 2);
        vga_buf += WIDTH/2;
        *vga_buf = COMPOSABLE_EOL_ALIGN << 16;

        vga_buf = buffer->data;
        vga_buf[0] = (vga_buf[1] << 16) | COMPOSABLE_RAW_RUN;
        vga_buf[1] = (vga_buf[1] & 0xFFFF0000) | (WIDTH - 2);
        buffer->data_used = (WIDTH + 4) / 2;
        scanvideo_end_scanline_generation (buffer);
    }
}

void setup_video(void)
{
    scanvideo_setup(&vga_mode_128x128);
    scanvideo_timing_enable(true);
#ifdef DEBUG
    printf ("System clock speed %d kHz\n", clock_get_hz (clk_sys) / 1000);
#endif
}

/*
 * Set a pixel in x4 mode (128x128)
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
    x = x * 2;
    y = y * 2;
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

void core1_main()
{
    setup_video();
    render_loop();
}

void process_usb_commands()
{
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    int led_status = 0;

#if 0
    while (true)
    {
        int c = getchar();

        absolute_time_t abs_time = get_absolute_time();
        led_status = (to_ms_since_boot(abs_time) / 50) % 2;
        gpio_put(LED_PIN, led_status);
 
    }
#endif
    while (true)
    {
        int c = getchar();

        absolute_time_t abs_time = get_absolute_time();
        led_status = (to_ms_since_boot(abs_time) / 50) % 2;
        gpio_put(LED_PIN, led_status);

        switch (c & 0xF0)
        {
            case DAZ_VERSION:
            {
                static uint8_t buf[3];
                buf[0] = DAZ_VERSION | (DAZZLER_VERSION & 0x0F);
                buf[1] = FEAT_VIDEO | FEAT_FRAMEBUF;
                buf[2] = 0;
                fwrite(buf, 1, 3, stdout);
                break;
            }
            case DAZ_CTRL:
            {
                if ((c & 0x0F) == 0)
                {
                    uint8_t prev_dazzler_ctrl = dazzler_ctrl;
                    dazzler_ctrl = (uint8_t) getchar();
                    if (dazzler_ctrl != prev_dazzler_ctrl)
                    {
                        /* If Dazzler is turned on */
                        if (dazzler_ctrl & 0x80)
                        {
                            refresh_vram();
                        }
                        else /* Dazzler turned off */
                        {
                            /* blank screen */
                            memset(frame_buffer, 0, sizeof(frame_buffer));
                        }
                    }
                }
                break;
            }
            case DAZ_CTRLPIC:
            {
                uint8_t prev_picture_ctrl = dazzler_picture_ctrl;

                if ((c & 0x0F) == 0)
                {
                    dazzler_picture_ctrl = (uint8_t) getchar();

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
                break;
            }
            case DAZ_MEMBYTE:
            {
                int addr = (c & 0x0F) * 256 + (uint8_t) getchar();
                uint8_t value = (uint8_t) getchar();
                set_vram(addr, value, false);
                break;
            }
            case DAZ_FULLFRAME:
            {
                if((c & 0x06) == 0)
                {
                    int addr = (c & 0x08) * 256;
                    int count = (c & 0x01) ? 2048 : 512;

                    for (int i = 0 ; i < count ; i++)
                    {
                        uint8_t value = (uint8_t) getchar();
                        set_vram(addr + i, value, false);
                    }
                }
                break;
            }
        }
    }
}

/* TODO: No longer used due to loop unrolling */
// uint8_t bit_masks[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

void set_vram(int addr, uint8_t value, bool refresh)
{
#ifdef DEBUG
    printf ("set_vram(%d,%x)\n", addr, value);
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
//            printf("Mode = 128x128m");
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

            set_pixel_64(x + 0, y, value & 0x0F);
            set_pixel_64(x + 1, y, value >> 4);
            break;
        }
        case mode_32x32c:
        {
            int y = addr * 2 / 32;
            int x = (addr * 2) % 32;
            set_pixel_32(x + 0, y, value & 0x0F);
            set_pixel_32(x + 1, y, value >> 4);
            break;
        }
    }
}

void refresh_vram()
{
    for (int i = 0 ; i < 2048 ; i++)
    {
        set_vram(i, raw_frame[i], false);
    }
}

int main(void)
{
    /* 1024x768 mode requires a system clock of 130MHz */
    set_sys_clock_khz(130000, true);
    stdio_init_all();

    memset(frame_buffer, 0, sizeof(frame_buffer));

    int clr = 0;
 #if 0
    for (int x = 0 ; x < WIDTH ; x++)
    {
        for (int y = 0 ; y < HEIGHT ; y++)
        {
            set_pixel_128(x,y,clr++ % 16);
        }
    }
    for (int x = 0 ; x < 32 ; x++)
    {
        for (int y = 0 ; y < 32 ; y++)
        {
            set_pixel_32(x,y,clr++%16);
        }
    }
#else
    sleep_ms(1000);
    
//    dazzler_picture_ctrl = 0b00110000;
//    dazzler_picture_ctrl = 0b01011100;
//    refresh_vram();
//    set_vram(2040, 0xFF);

#if 0
    for (int i = 0 ; i < 2048 ; i++)
    {
        set_vram(i, 0xCC, false);
    }
#endif
#endif
    multicore_launch_core1(core1_main);
#ifdef DEBUG
    printf("processing serial commands\n");
#endif
    process_usb_commands();
//    while(true){}
}