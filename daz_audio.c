
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

/* The PICO I2S sound library uses DMA for transferring samples from a "producer" buffer of PCM samples to the I2S PIO program. 
 * While technically we could create output in that format, it is much more complicated. Instead we use a timer to directly 
 * write the audio values to the PIO's OSR queue.
 * The Altair-Duino sends audio in "delay", "sample" format, where delay means how long to play the *previous* sample (in us).
*/
#include <stdio.h>
#include <math.h>

#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"

#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include "pico/audio_i2s.h"
#include "audio_i2s.pio.h"
#include "pico/binary_info.h"
#include "hardware/pio.h"


#define DEBUG_INFO  DEBUG_AUDIO
#define DEBUG_TRACE TRACE_AUDIO
#include "debug.h"

#define AUDIO_QUEUE_LEN     512         /* Keep a buffer of 512 samples. This seems to be enough to minimise overflows 
                                           without delaying audio too much */
#define HWALARM_NUM         2           /* Dedicated ardware alarm to use */
#define AUDIO_SAMPLE_RATE   48000       /* 48kHz audio */
#define ALARM_FREQ          (1000000 / AUDIO_SAMPLE_RATE)   /* 20us for 48kHz, which is slightly fast, but works */
#define audio_pio __CONCAT(pio, PICO_AUDIO_I2S_PIO)
#define GPIO_FUNC_PIOx __CONCAT(GPIO_FUNC_PIO, PICO_AUDIO_I2S_PIO)

bool play_audio_sample_cb(struct repeating_timer *t);

static uint32_t current_sample = 0;     /* The currently playing 16 bit PCM for L & R channels = 32 bits */
static bool have_delay[2];              /* Have we recevied a delay for how long to play this sample yet? */            
static uint16_t next_sample[2];         /* The next PCM sample value to play */    
static absolute_time_t next_delay[2];   /* how long to play the current (not next) PCM sample */

static alarm_pool_t *audio_alarm_pool;  /* Create a separate alarm pool for audio which can't be shared */
static queue_t chan0_queue;             /* Queue of audio samples for left channel */
static queue_t chan1_queue;             /* Queue of audio samples for right channel */

/* Set PIO State machine frequency to be multiple of sample frequency. 
 * Required so that state machine clocks out the data bits at the correct rate
 * Divider is in 1/256th of clock cycle. 2 PIO clock cycles per output and 32 bit to output
 * equals system_clock_frequency * 256 / 32 / 2 / sample_freq
 * equals system_clock_frequency * 4 / sample_freq */
static void dazzler_update_pio_frequency(uint32_t sample_freq, uint pio_sm) {
    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    assert(system_clock_frequency < 0x40000000);
    uint32_t divider = system_clock_frequency * 4 / sample_freq;
    assert(divider < 0x1000000);
    pio_sm_set_clkdiv_int_frac(audio_pio, pio_sm, divider >> 8u, divider & 0xffu);
}

/* Setup I2S Audio pins and load PIO program */
static void dazzler_audio_i2s_setup(const audio_format_t *intended_audio_format,
                                               const audio_i2s_config_t *config) {
    uint func = GPIO_FUNC_PIOx;
    gpio_set_function(config->data_pin, func);
    gpio_set_function(config->clock_pin_base, func);
    gpio_set_function(config->clock_pin_base + 1, func);

    uint8_t sm = config->pio_sm;
    pio_sm_claim(audio_pio, sm);
    PRINT_INFO("I2S PIO = %d, sm = %d\n", (audio_pio == pio0) ? 0 : 1, sm);
    uint offset = pio_add_program(audio_pio, &audio_i2s_program);

    audio_i2s_program_init(audio_pio, sm, offset, config->data_pin, config->clock_pin_base);
}

/* Initialise I2S Audio */
void audio_init() {
    static repeating_timer_t audio_timer;

    /* Dazzler uses signed 8 bit data, but this is converted to signed 16 bit data */
    static audio_format_t audio_format = 
    {
        .format = AUDIO_BUFFER_FORMAT_PCM_S16,
        .sample_freq = AUDIO_SAMPLE_RATE,
        .channel_count = 2,
    };

    /* Make sure to used an unused DMA channel and PIO state machine */
    struct audio_i2s_config config = {
            .data_pin = PICO_AUDIO_I2S_DATA_PIN,
            .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
            .dma_channel = 1,
            .pio_sm = 3,
    };

    dazzler_audio_i2s_setup(&audio_format, &config);
    dazzler_update_pio_frequency(audio_format.sample_freq, config.pio_sm);
    pio_sm_set_enabled(audio_pio, config.pio_sm, true);


    queue_init(&chan0_queue, 4, AUDIO_QUEUE_LEN);
    queue_init(&chan1_queue, 4, AUDIO_QUEUE_LEN);
    audio_alarm_pool = alarm_pool_create(HWALARM_NUM, 1);

    /* Wait for alarms system to fully initialise, otherwise can get occasional assertion failures */
    sleep_ms(100);
    alarm_pool_add_repeating_timer_us(audio_alarm_pool,-ALARM_FREQ, play_audio_sample_cb, NULL, &audio_timer);
}

/* Called every 20us for 48kHz audio. Find the correct PCM value to play and output it to it I2S PIO state machine.
 * The I2S audio chip is clocked from the data clock so this must be called every ~20.833us, otherwise audio chip 
 * will fail to output anything. We use 20 us, which seems to work */
bool __time_critical_func(play_audio_sample_cb)(struct repeating_timer *t) 
{
    absolute_time_t current_time = get_absolute_time();

    /* If current PCM sample has played long enough */
    if (absolute_time_diff_us(next_delay[0], current_time) >= 0)
    {
        /* update current sample from store next sample */
        current_sample = (current_sample & 0xffff00000) | next_sample[0];

        uint32_t delay_and_sample;
        if (queue_try_remove(&chan0_queue, &delay_and_sample))
        {
            /* If another sample is queued, then set next_delay (which is how long to play the current sample)
             * and next_sample (which is the next sample to play) */
            next_delay[0] = delayed_by_us(current_time, delay_and_sample >> 16);
            next_sample[0] = delay_and_sample & 0x0000ffff;
            have_delay[0] = true;
        }
        else
        {
            /* Otherwise the current sample will play once and then queue will time out */
            have_delay[0] = false;
            current_sample &= 0xffff0000;
        }
    }
    else if (!have_delay[0])
    {
        /* Queue has timed out */
        uint32_t delay_and_sample;
        if (queue_try_remove(&chan0_queue, &delay_and_sample))
        {
            /* If a new sample has arrived, then wait 5ms to build up more samples */
            have_delay[0] = true;
            next_delay[0] = delayed_by_us(current_time, 5000);
            next_sample[0] = delay_and_sample & 0x0000ffff;
        }
    }
    /* Same logic for the right channel */
    if (absolute_time_diff_us(next_delay[1], current_time) >= 0)
    {
        current_sample = (current_sample & 0x0000ffff) | (next_sample[1]<< 16);

        uint32_t delay_and_sample;
        if (queue_try_remove(&chan1_queue, &delay_and_sample))
        {
            next_delay[1] = delayed_by_us(current_time, delay_and_sample >> 16);
            next_sample[1] = delay_and_sample & 0x0000ffff;
            have_delay[1] = true;
        }
        else
        {
            have_delay[1] = false;
            current_sample &= 0x0000ffff;
        }
    }
    else if (!have_delay[1])
    {
        uint32_t delay_and_sample;
        if (queue_try_remove(&chan1_queue, &delay_and_sample))
        {
            have_delay[1] = true;
            next_delay[1] = delayed_by_us(current_time, 5000);
            next_sample[1] = delay_and_sample & 0x0000ffff;
        }
    }
    /* Finally send the sample to the state machine */
    pio_sm_put(audio_pio, 3, current_sample);
    return true;
}

/* Add a PCM sample to the left or right channel. 
 * Sample is the 8 bit sample to play 
 * Delay in microseconds is how long to play the previous sample */
void audio_add_sample(uint8_t channel, uint16_t delay_us, uint8_t sample)
{
    if (channel == 0)
    {
        /* Convert 8 but sample to 16 bit sample, required by I2S audio */
        uint32_t value = (delay_us << 16) | (sample << 8);
        if (queue_try_add(&chan0_queue, &value) == false)
        {
            PRINT_INFO("Chan0 audio queue full\n");
        }
    }
    else
    {
        uint32_t value = (delay_us << 16) | (sample << 8);
        if (queue_try_add(&chan1_queue, &value) == false)
        {
            PRINT_INFO("Chan1 audio queue full\n");
        }
    }
    return;
}

/* Test audio output */
#if DAZAUDIO_STANDALONE
void audio_add_sample(uint8_t channel, uint16_t delay_us, uint8_t sample);
int main() {

    stdio_init_all();
    audio_init();

    absolute_time_t sample_time = make_timeout_time_us(timeout);
    int samples[2] = { 0x7f, 0x81 };
    int sample_nr = 0;
    while (true) {
        absolute_time_t abs_time = get_absolute_time();        
        if(absolute_time_diff_us(abs_time, sample_time) <= 0)
        {
            audio_add_sample(0, 100, samples[sample_nr]);
            sample_nr ^= 1;
            sample_time = make_timeout_time_us(timeout);
        }
    }
    return 0;
}

#endif

