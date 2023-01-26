#include "pico.h"
#include "pico/stdlib.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"

#include <string.h>
#include <stdio.h>
/* 
 * Dazzler resolutions are:
 * 128x128, 64x64 and 32x32
 * We choose a 1024x768 resolution as it is a multiple of 128
 */
#define WIDTH   128
#define HEIGHT  128
#define NUMCLR  16

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
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  64u,   64u,   64u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u,   0u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u, 255u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u, 255u,   0u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u,   0u, 255u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u,   0u, 255u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  0u, 255u, 255u),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(255u, 255u, 255u)
};

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
uint16_t     frame_buffer[WIDTH * HEIGHT];

void __time_critical_func(render_loop) (void)
{
    printf ("Starting render\n");
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
    printf ("System clock speed %d kHz\n", clock_get_hz (clk_sys) / 1000);
}

/*
 * Set a pixel in x4 mode (128x128)
 */
void set_pixel_128(int x, int y, int colour)
{
    colour &= 0x0F;
    if (x >= 0 && x < WIDTH && y >=0 && y < HEIGHT)
    {
        frame_buffer[y * WIDTH + x] = colours[colour];
    }
}

void set_pixel_64(int x, int y, int colour)
{
    x = x * 2;
    y = y * 2;
    int start = y * WIDTH + x;
    if (x >= 0 && x < WIDTH && y >=0 && y < HEIGHT)
    {
        frame_buffer[start] = colours[colour];
        frame_buffer[start + 1] = colours[colour];
        start += WIDTH;
        frame_buffer[start] = colours[colour];
        frame_buffer[start + 1] = colours[colour];
    }
}

void set_pixel_32(int x, int y, int colour)
{
    x = x * 4;
    y = y * 4;
    int start = y * WIDTH + x;
    if (x >= 0 && x < WIDTH && y >=0 && y < HEIGHT)
    {
        frame_buffer[start] = colours[colour];
        frame_buffer[start + 1] = colours[colour];
        frame_buffer[start + 2] = colours[colour];
        frame_buffer[start + 3] = colours[colour];
        start += WIDTH;
        frame_buffer[start] = colours[colour];
        frame_buffer[start + 1] = colours[colour];
        frame_buffer[start + 2] = colours[colour];
        frame_buffer[start + 3] = colours[colour];
        start += WIDTH;
        frame_buffer[start] = colours[colour];
        frame_buffer[start + 1] = colours[colour];
        frame_buffer[start + 2] = colours[colour];
        frame_buffer[start + 3] = colours[colour];
        start += WIDTH;
        frame_buffer[start] = colours[colour];
        frame_buffer[start + 1] = colours[colour];
        frame_buffer[start + 2] = colours[colour];
        frame_buffer[start + 3] = colours[colour];
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
#else
    for (int x = 0 ; x < 32 ; x++)
    {
        for (int y = 0 ; y < 32 ; y++)
        {
            set_pixel_32(x,y,clr++%16);
        }
    }
#endif
    setup_video();
    render_loop();
}