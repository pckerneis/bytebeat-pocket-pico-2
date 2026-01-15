#include "audio.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "pico/stdlib.h"

#define AUDIO_PIN 0

static uint slice;
static bool audio_enabled = false;

void audio_init() {
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(AUDIO_PIN);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, 1.0f);
    pwm_config_set_wrap(&cfg, 255); // 8-bit audio

    pwm_init(slice, &cfg, true);
    
    // Start with silence (center value)
    pwm_set_gpio_level(AUDIO_PIN, 128);
}

void audio_enable(bool enable) {
    audio_enabled = enable;
    if (!enable) {
        // Set to silence when disabled
        pwm_set_gpio_level(AUDIO_PIN, 128);
    }
}

inline void audio_write(uint8_t v) {
    if (audio_enabled) {
        pwm_set_gpio_level(AUDIO_PIN, v);
    }
}
