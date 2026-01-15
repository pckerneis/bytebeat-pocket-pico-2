#pragma once
#include <stdint.h>
#include <stdbool.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define TEXT_BUFFER_SIZE 256

// UI state
extern bool isPlaying;
extern uint8_t current_slot;
extern volatile bool oledDirty;
extern bool needsRecompile;
extern bool needsResetT;

// Function prototypes
void ui_init(void);
void ui_update(void);
void ui_set_expression(const char* expr);
void ui_show_toaster(const char* msg, uint32_t duration_ms);
void ui_handle_play_stop(void);
