#ifndef _AUDIO_H_

#include <stdint.h>

void audio_init(void) ;
void audio_add_sample(uint8_t channel, uint16_t delay_us, uint8_t sample);

#endif