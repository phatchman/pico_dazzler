
/**
 */

#include <stdio.h>
#include <math.h>

#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"

#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include "pico/audio_i2s.h"
#include "pico/binary_info.h"
#include "hardware/pio.h"

#define audio_pio __CONCAT(pio, PICO_AUDIO_I2S_PIO)
bi_decl(bi_3pins_with_names(PICO_AUDIO_I2S_DATA_PIN, "I2S DIN", PICO_AUDIO_I2S_CLOCK_PIN_BASE, "I2S BCK", PICO_AUDIO_I2S_CLOCK_PIN_BASE+1, "I2S LRCK"));

struct audio_buffer_pool *producer_pool = 0;
static absolute_time_t audio_delay_time = 0;
static queue_t audio_queue;
static repeating_timer_t audio_timer;
static volatile uint32_t current_audio_sample = 0;

bool play_audio_sample_cb(struct repeating_timer *t);

void audio_init() {

    static audio_format_t audio_format = {
            .format = AUDIO_BUFFER_FORMAT_PCM_U16,
            .sample_freq = 48000,
            .channel_count = 2,
    };

    static struct audio_buffer_format producer_format = {
            .format = &audio_format,
            .sample_stride = 2
        };

    producer_pool = audio_new_producer_pool(&producer_format, 16, 1); // todo correct size
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

    output_format = audio_i2s_setup(&audio_format, &config);
    if (!output_format) {
        panic("PicoAudio: Unable to open audio device.\n");
    }

    ok = audio_i2s_connect(producer_pool);
    assert(ok);
    audio_i2s_set_enabled(true);

    queue_init(&audio_queue, 4, 32);
//    add_repeating_timer_us(-21, play_audio_sample_cb, NULL, &audio_timer);
    add_repeating_timer_us(-20, play_audio_sample_cb, NULL, &audio_timer);
}


bool play_audio_sample_cb(struct repeating_timer *t) {
//    printf("Play: %X\n", current_audio_sample);
    pio_sm_put(audio_pio, 3, current_audio_sample);
    return true;

    int on = true;
    static absolute_time_t timer = 0;
    if (timer == 0)
    {
        timer = make_timeout_time_us(10000);
    }
    if (get_absolute_time() > timer)
    {
        pio_sm_put(audio_pio, 3, current_audio_sample);
    }
    else
    {
        pio_sm_put(audio_pio, 3, 0);  
    }
    return true;
}

void audio_add_sample(uint8_t channel, uint16_t delay_us, uint8_t sample)
{
    if (channel == 0)
    {
        uint32_t new_sample = current_audio_sample & 0x0000ffff;
        new_sample |= (sample << 24);
//        printf("New Sample: %X:%X:%X:%X\n", sample, (uint32_t) sample << 24, new_sample, current_audio_sample);
        current_audio_sample = new_sample;
    }
    else
    {
        uint32_t new_sample = current_audio_sample & 0xffff0000;
        new_sample |= (sample << 8);
//        printf("New Sample: %X:%X:%X\n", sample, new_sample, current_audio_sample);
        current_audio_sample = new_sample;
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
        for (int i = 0 ; i < 500 ; i++)
        {
            absolute_time_t abs_time = get_absolute_time();        
            if(abs_time > sample_time)
            {
                audio_add_sample(0, 50, samples[sample_nr]);
                sample_nr ^= 1;
                sample_time = make_timeout_time_us(timeout);
            }
        }
    }
    return 0;
}

#endif

