#include "ui.h"
#include "rpn_vm.h"
#include "audio.h"
#include "display.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

// UI state
bool isPlaying = false;
uint8_t current_slot = 0;

void ui_init(void) {
    // Initialize display first
    display_init();
}

void ui_handle_play_stop(void) {
    isPlaying = !isPlaying;
    if (isPlaying) {
        audio_enable(true);
        printf("Audio started - Playing: %s\n", textBuffer);
        show_toaster("Audio started");
    } else {
        audio_enable(false);
        printf("Audio stopped\n");
        show_toaster("Audio stopped");
    }
    oledDirty = true;
}

void ui_set_expression(const char* expr) {
    if (strlen(expr) >= TEXT_BUFFER_SIZE) return;
    
    strcpy(textBuffer, expr);
    text_len = strlen(textBuffer);
    cursor = text_len;
    needsRecompile = true;
    
    printf("Expression set: %s\n", textBuffer);
    show_toaster("Expression set");
    oledDirty = true;
}

void ui_show_toaster(const char* msg, uint32_t duration_ms) {
    show_toaster(msg);
    printf("Toaster: %s\n", msg);
}

void ui_update(void) {
    // Update display if needed
    display_update();
}
