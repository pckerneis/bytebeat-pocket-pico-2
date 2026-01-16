#pragma once
#include <stdint.h>
#include <stdbool.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define TEXT_BUFFER_SIZE 256

// Display state
extern volatile bool oledDirty;
extern bool toasterVisible;
extern char toasterMsg[32];
extern uint32_t toasterStartTime;
#define TOASTER_DURATION 2000 // ms

// Function prototypes
void display_init(void);
void display_update(void);
void display_clear(void);
void display_set_cursor(uint8_t x, uint8_t y);
void display_print(const char* text);
void show_toaster(const char* msg);
void draw_error_banner(const char* error);
void draw_expression_editor(void);
