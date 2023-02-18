
/**
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

#define DEBUG

#define audio_pio __CONCAT(pio, PICO_AUDIO_I2S_PIO)
#define GPIO_FUNC_PIOx __CONCAT(GPIO_FUNC_PIO, PICO_AUDIO_I2S_PIO)

bool play_audio_sample_cb(struct repeating_timer *t);

static uint32_t current_sample = 0;
static bool have_delay[2];
static uint16_t next_sample[2];
static absolute_time_t next_delay[2];

static alarm_pool_t *audio_alarm_pool;
static queue_t chan0_queue;
static queue_t chan1_queue;
static audio_buffer_pool_t *producer_pool;

extern struct {
    audio_buffer_t *playing_buffer;
    uint32_t freq;
    uint8_t pio_sm;
    uint8_t dma_channel;
} shared_state;

static void dazzler_update_pio_frequency(uint32_t sample_freq) {
    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    assert(system_clock_frequency < 0x40000000);
    uint32_t divider = system_clock_frequency * 4 / sample_freq; // avoid arithmetic overflow
    assert(divider < 0x1000000);
    pio_sm_set_clkdiv_int_frac(audio_pio, shared_state.pio_sm, divider >> 8u, divider & 0xffu);
    shared_state.freq = sample_freq;
}

const audio_format_t *dazzler_audio_i2s_setup(const audio_format_t *intended_audio_format,
                                               const audio_i2s_config_t *config) {
    uint func = GPIO_FUNC_PIOx;
    gpio_set_function(config->data_pin, func);
    gpio_set_function(config->clock_pin_base, func);
    gpio_set_function(config->clock_pin_base + 1, func);

    uint8_t sm = shared_state.pio_sm = config->pio_sm;
    pio_sm_claim(audio_pio, sm);
    printf("PIO = %d, sm = %d\n", (audio_pio == pio0) ? 0 : 1, sm);
    uint offset = pio_add_program(audio_pio, &audio_i2s_program);

    audio_i2s_program_init(audio_pio, sm, offset, config->data_pin, config->clock_pin_base);

    return intended_audio_format;
}


void audio_init() {
    static repeating_timer_t audio_timer;

    static audio_format_t audio_format = 
    {
        .format = AUDIO_BUFFER_FORMAT_PCM_S16,
        .sample_freq = 48000,
        .channel_count = 2,
    };

    static struct audio_buffer_format producer_format = 
    {
        .format = &audio_format,
        .sample_stride = 2
    };

//    producer_pool = audio_new_producer_pool(&producer_format, 16, 1); // todo correct size
    bool __unused ok;
    const struct audio_format *output_format;
    printf("PICO_AUDIO_I2S_DATA_PIN[%d] PICO_AUDIO_I2S_CLOCK_PIN_BASE[%d]\n", 
            PICO_AUDIO_I2S_DATA_PIN, PICO_AUDIO_I2S_CLOCK_PIN_BASE);
    printf("PICO_ON_DEVICE[%d]\n", PICO_ON_DEVICE);
    struct audio_i2s_config config = {
            .data_pin = PICO_AUDIO_I2S_DATA_PIN,
            .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
            .dma_channel = 1,
            .pio_sm = 3,
    };

#if 1
    dazzler_audio_i2s_setup(&audio_format, &config);
#else   
    output_format = audio_i2s_setup(&audio_format, &config);
    if (!output_format) {
        panic("PicoAudio: Unable to open audio device.\n");
    }
#endif
#if 1
    dazzler_update_pio_frequency(producer_format.format->sample_freq);
#else
    ok = audio_i2s_connect(producer_pool);
    assert(ok);
#endif
#if 1
    pio_sm_set_enabled(audio_pio, shared_state.pio_sm, true);
#else
    audio_i2s_set_enabled(true);
#endif
    queue_init(&chan0_queue, 4, 512);
    queue_init(&chan1_queue, 4, 512);
    audio_alarm_pool = alarm_pool_create(2, 2);

//    add_repeating_timer_us(-21, play_audio_sample_cb, NULL, &audio_timer);
//    alarm_pool_add_repeating_timer_us(audio_alarm_pool,-20, play_audio_sample_cb, NULL, &audio_timer);
    sleep_ms(100);
    alarm_pool_add_repeating_timer_us(audio_alarm_pool,-20, play_audio_sample_cb, NULL, &audio_timer);
}

bool __time_critical_func(play_audio_sample_cb)(struct repeating_timer *t) 
{
    absolute_time_t current_time = get_absolute_time();

    if (absolute_time_diff_us(next_delay[0], current_time) >= 0)
    {
        current_sample = (current_sample & 0xffff00000) | next_sample[0];

        uint32_t delay_and_sample;
        if (queue_try_remove(&chan0_queue, &delay_and_sample))
        {
            next_delay[0] = delayed_by_us(current_time, delay_and_sample >> 16);
            next_sample[0] = delay_and_sample & 0x0000ffff;
            have_delay[0] = true;
        }
        else
        {
            have_delay[0] = false;
            current_sample &= 0xffff0000;
        }
    }
    else if (!have_delay[0])
    {
        uint32_t delay_and_sample;
        if (queue_try_remove(&chan0_queue, &delay_and_sample))
        {
            have_delay[0] = true;
            next_delay[0] = delayed_by_us(current_time, 5000);
            next_sample[0] = delay_and_sample & 0x0000ffff;
        }
    }

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
    pio_sm_put(audio_pio, 3, current_sample);
    return true;
}

void audio_add_sample(uint8_t channel, uint16_t delay_us, uint8_t sample)
{
    if (channel == 0)
    {
        uint32_t value = (delay_us << 16) | (sample << 8);
        if (queue_try_add(&chan0_queue, &value) == false)
        {
#ifdef DEBUG
            printf("Chan0 audio queue full\n");
#endif
        }
    }
    else
    {
        uint32_t value = (delay_us << 16) | (sample << 8);
        if (queue_try_add(&chan1_queue, &value) == false)
        {
#ifdef DEBUG
            printf("Chan1 audio queue full\n");
#endif
        }
    }
    return;
}

#if DAZAUDIO_STANDALONE
void audio_add_sample(uint8_t channel, uint16_t delay_us, uint8_t sample);
int main() {

    stdio_init_all();

    audio_init();

    int timeout = 2336;
    absolute_time_t sample_time = make_timeout_time_us(timeout);
//    uint16_t sample = (50*256) << 16 | (50*256);
    int samples[2] = { 0x7f, 0x81 };
    int sample_nr = 0;
    while (true) {
        absolute_time_t abs_time = get_absolute_time();        
        if(absolute_time_diff_us(abs_time, sample_time) <= 0)
        {
            audio_add_sample(0, 50, samples[sample_nr]);
            sample_nr ^= 1;
            sample_time = make_timeout_time_us(timeout);
        }
    }
    return 0;
}

#endif

