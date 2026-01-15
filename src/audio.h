#pragma once
#include <stdint.h>
#include <stdbool.h>

void audio_init(void);
void audio_enable(bool enable);
void audio_write(uint8_t v);
