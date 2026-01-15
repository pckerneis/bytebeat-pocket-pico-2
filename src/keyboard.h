#pragma once
#include <stdint.h>
#include <stdbool.h>

// Matrix configuration
#define ROWS 4
#define COLS 5
#define KEY_COUNT 20

// Key indices for special keys
#define KEY_FN1 8   // row 1, col 3
#define KEY_FN2 9   // row 1, col 4
#define KEY_MEM 3   // row 0, col 3

// Key modes
typedef enum {
    MODE_BASE,
    MODE_FN1,
    MODE_FN2,
    MODE_MEM
} KeyMode;

// Actions
typedef enum {
    ACT_NONE,
    
    // Navigation
    ACT_LEFT,
    ACT_RIGHT,
    
    // Editing
    ACT_DEL,
    ACT_ENTER,
    
    // Digits
    ACT_DIGIT_0, ACT_DIGIT_1, ACT_DIGIT_2, ACT_DIGIT_3, ACT_DIGIT_4,
    ACT_DIGIT_5, ACT_DIGIT_6, ACT_DIGIT_7, ACT_DIGIT_8, ACT_DIGIT_9,
    
    // Operators
    ACT_ADD, ACT_SUB, ACT_MUL, ACT_DIV, ACT_MOD,
    ACT_AND, ACT_OR, ACT_XOR, ACT_NOT,
    ACT_GT, ACT_LT, ACT_EQ,
    ACT_DOT, ACT_QUOTE, ACT_COLON, ACT_SEMICOLON, ACT_QUESTION,
    ACT_0B, ACT_0X,
    
    // Literals
    ACT_T,
    
    // Parentheses
    ACT_LEFT_PAREN, ACT_RIGHT_PAREN,
    
    // Alpha
    ACT_ALPHA_A, ACT_ALPHA_B, ACT_ALPHA_C,
    ACT_ALPHA_D, ACT_ALPHA_E, ACT_ALPHA_F,
    
    // Modes
    ACT_FN1, ACT_FN2, ACT_MEM,
    
    // Presets
    ACT_PRESET_1, ACT_PRESET_2, ACT_PRESET_3,
    ACT_PRESET_4, ACT_PRESET_5, ACT_PRESET_6,
    ACT_PRESET_7, ACT_PRESET_8, ACT_PRESET_9,
    ACT_PRESET_DEC, ACT_PRESET_INC,
    
    ACT_SAVE
} Action;

// Function prototypes
void keyboard_init(void);
void keyboard_scan(void);
uint8_t keyboard_get_pressed_key(void);
bool keyboard_is_key_pressed(uint8_t key);
Action keyboard_resolve_action(uint8_t key);
bool keyboard_execute_action(Action action);

// External state
extern KeyMode currentMode;
extern uint8_t keyStates[KEY_COUNT];
extern uint8_t keyStatesPrev[KEY_COUNT];
