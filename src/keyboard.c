#include "keyboard.h"
#include "rpn_vm.h"
#include "ui.h"
#include "preset.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>

// GPIO pin configuration
// Columns are outputs (driven LOW to scan) - DRIVE side
const uint8_t col_pins[COLS] = {20, 21, 22, 26, 27};
// Rows are inputs (with pullups) - SENSE side
const uint8_t row_pins[ROWS] = {16, 17, 18, 19};

// Key state tracking
uint8_t keyStates[KEY_COUNT] = {0};

// Current mode
KeyMode currentMode = MODE_BASE;

/*          
┌───┬───┬───┬───┬───┐
│ 7 │ 8 │ 9 │MEM│DEL│
├───┼───┼───┼───┼───┤
│ 4 │ 5 │ 6 │FN1│FN2│
├───┼───┼───┼───┼───┤
│ 1 │ 2 │ 3 │0b │0x │
├───┼───┼───┼───┼───┤
│ ← │ 0 │ → │ . │ ▸ │
└───┴───┴───┴───┴───┘
*/
const Action baseLayer[KEY_COUNT] = {
    ACT_DIGIT_7, ACT_DIGIT_8, ACT_DIGIT_9, ACT_MEM, ACT_DEL,
    ACT_DIGIT_4, ACT_DIGIT_5, ACT_DIGIT_6, ACT_FN1, ACT_FN2,
    ACT_DIGIT_1, ACT_DIGIT_2, ACT_DIGIT_3, ACT_0B,  ACT_0X,
    ACT_LEFT,    ACT_DIGIT_0, ACT_RIGHT,   ACT_DOT, ACT_ENTER
};

/*
┌───┬───┬───┬───┬───┐
│ + │ - │ * │MEM│DEL│
├───┼───┼───┼───┼───┤
│ / │ % │ ~ │FN1│FN2│
├───┼───┼───┼───┼───┤
│ & │ | │ ^ │ < │ > │
├───┼───┼───┼───┼───┤
│ ← │ t │ → │ = │ ▸ │
└───┴───┴───┴───┴───┘
*/
const Action fn1Layer[KEY_COUNT] = {
    ACT_ADD, ACT_SUB, ACT_MUL, ACT_MEM, ACT_DEL,
    ACT_DIV, ACT_MOD, ACT_NOT, ACT_FN1, ACT_FN2,
    ACT_AND, ACT_OR,  ACT_XOR, ACT_LT,  ACT_GT,
    ACT_LEFT, ACT_T,  ACT_RIGHT, ACT_EQ, ACT_ENTER
};

/*
┌───┬───┬───┬───┬───┐
│ a │ b │ c │MEM│DEL│
├───┼───┼───┼───┼───┤
│ d │ e │ f │FN1│FN2│
├───┼───┼───┼───┼───┤
│ ? │ : │ " │ ( │ ) │
├───┼───┼───┼───┼───┤
│ ← │   │ → │ ; │ ▸ │
└───┴───┴───┴───┴───┘
*/
const Action fn2Layer[KEY_COUNT] = {
    ACT_ALPHA_A,  ACT_ALPHA_B, ACT_ALPHA_C, ACT_MEM,        ACT_DEL,
    ACT_ALPHA_D,  ACT_ALPHA_E, ACT_ALPHA_F, ACT_FN1,        ACT_FN2,
    ACT_QUESTION, ACT_COLON,   ACT_QUOTE,   ACT_LEFT_PAREN, ACT_RIGHT_PAREN,
    ACT_LEFT,     ACT_NONE,    ACT_RIGHT,   ACT_SEMICOLON,  ACT_ENTER
};

/*
┌───┬───┬───┬───┬───┐
│P7 │P8 │P9 │MEM│DEL│
├───┼───┼───┼───┼───┤
│P4 │P5 │P6 │FN1│FN2│
├───┼───┼───┼───┼───┤
│P1 │P2 │P3 │   │   │
├───┼───┼───┼───┼───┤
│P- │   │P+ │   │SAV│
└───┴───┴───┴───┴───┘
*/
const Action memLayer[KEY_COUNT] = {
    ACT_PRESET_7,   ACT_PRESET_8, ACT_PRESET_9, ACT_MEM,        ACT_DEL,
    ACT_PRESET_4,   ACT_PRESET_5, ACT_PRESET_6, ACT_FN1,        ACT_FN2,
    ACT_PRESET_1,   ACT_PRESET_2, ACT_PRESET_3, ACT_NONE,       ACT_NONE,
    ACT_PRESET_DEC, ACT_NONE,     ACT_PRESET_INC, ACT_NONE,    ACT_SAVE
};

void keyboard_init(void) {
    // Configure column pins as outputs (idle HIGH) - DRIVE side
    for (int c = 0; c < COLS; c++) {
        gpio_init(col_pins[c]);
        gpio_set_dir(col_pins[c], GPIO_OUT);
        gpio_put(col_pins[c], 1); // Idle HIGH
    }
    
    // Configure row pins as inputs with pullups - SENSE side
    for (int r = 0; r < ROWS; r++) {
        gpio_init(row_pins[r]);
        gpio_set_dir(row_pins[r], GPIO_IN);
        gpio_pull_up(row_pins[r]);
    }
    
    printf("Keyboard matrix initialized\n");
}

void keyboard_scan(void) {
    // Scan all keys - drive each column LOW and sense rows
    for (int c = 0; c < COLS; c++) {
        gpio_put(col_pins[c], 0); // Drive column LOW
        sleep_us(10); // Settle time for stable reading
        
        for (int r = 0; r < ROWS; r++) {
            uint8_t keyIndex = r * COLS + c;
            keyStates[keyIndex] = (gpio_get(row_pins[r]) == 0) ? 1 : 0;
        }
        
        gpio_put(col_pins[c], 1); // Return to HIGH
        sleep_us(5); // Small delay before next column
    }
}

uint8_t keyboard_get_pressed_key(void) {
    // Return first currently pressed key (no edge detection)
    for (uint8_t i = 0; i < KEY_COUNT; i++) {
        if (keyStates[i] == 1) {
            return i; // Key is pressed
        }
    }
    return 255; // No key pressed
}

bool keyboard_is_key_pressed(uint8_t key) {
    return (key < KEY_COUNT) ? (keyStates[key] == 1) : false;
}

Action keyboard_resolve_action(uint8_t key) {
    if (key >= KEY_COUNT) return ACT_NONE;
    
    if (currentMode == MODE_FN1) {
        return fn1Layer[key];
    }
    
    if (currentMode == MODE_FN2) {
        return fn2Layer[key];
    }
    
    if (currentMode == MODE_MEM) {
        return memLayer[key];
    }
    
    return baseLayer[key];
}

// Editor functions
static bool insertChar(char c) {
    if (text_len >= TEXT_BUFFER_SIZE - 1) return false;
    
    for (int i = text_len; i > cursor; i--) {
        textBuffer[i] = textBuffer[i - 1];
    }
    
    textBuffer[cursor] = c;
    text_len++;
    cursor++;
    textBuffer[text_len] = '\0';
    needsRecompile = true;
    return true;
}

static bool insertString(const char* text) {
    uint8_t len = strlen(text);
    if (text_len + len >= TEXT_BUFFER_SIZE) return false;
    for (uint8_t i = 0; i < len; i++) {
        insertChar(text[i]);
    }
    return true;
}

static bool deleteChar(void) {
    if (cursor == 0 || text_len == 0) return false;
    
    for (int i = cursor - 1; i < text_len - 1; i++) {
        textBuffer[i] = textBuffer[i + 1];
    }
    
    text_len--;
    cursor--;
    textBuffer[text_len] = '\0';
    needsRecompile = true;
    return true;
}

bool keyboard_execute_action(Action action) {
    switch (action) {
        case ACT_NONE:
            return false;
            
        // Navigation
        case ACT_LEFT:
            if (cursor > 0) cursor--;
            return true;
            
        case ACT_RIGHT:
            if (cursor < text_len) cursor++;
            return true;
            
        // Editing
        case ACT_DEL:
            return deleteChar();
            
        case ACT_ENTER:
            ui_handle_play_stop();
            return true;
            
        // Digits
        case ACT_DIGIT_0: return insertChar('0');
        case ACT_DIGIT_1: return insertChar('1');
        case ACT_DIGIT_2: return insertChar('2');
        case ACT_DIGIT_3: return insertChar('3');
        case ACT_DIGIT_4: return insertChar('4');
        case ACT_DIGIT_5: return insertChar('5');
        case ACT_DIGIT_6: return insertChar('6');
        case ACT_DIGIT_7: return insertChar('7');
        case ACT_DIGIT_8: return insertChar('8');
        case ACT_DIGIT_9: return insertChar('9');
            
        // Operators
        case ACT_ADD: return insertChar('+');
        case ACT_SUB: return insertChar('-');
        case ACT_MUL: return insertChar('*');
        case ACT_DIV: return insertChar('/');
        case ACT_MOD: return insertChar('%');
        case ACT_AND: return insertChar('&');
        case ACT_OR:  return insertChar('|');
        case ACT_XOR: return insertChar('^');
        case ACT_NOT: return insertChar('~');
        case ACT_GT:  return insertChar('>');
        case ACT_LT:  return insertChar('<');
        case ACT_EQ:  return insertChar('=');
        case ACT_DOT: return insertChar('.');
        case ACT_QUOTE: return insertChar('"');
        case ACT_COLON: return insertChar(':');
        case ACT_SEMICOLON: return insertChar(';');
        case ACT_QUESTION: return insertChar('?');
        case ACT_0B: return insertString("0b");
        case ACT_0X: return insertString("0x");
            
        // Literals
        case ACT_T: return insertChar('t');
            
        // Parentheses
        case ACT_LEFT_PAREN: return insertChar('(');
        case ACT_RIGHT_PAREN: return insertChar(')');
            
        // Alpha
        case ACT_ALPHA_A: return insertChar('a');
        case ACT_ALPHA_B: return insertChar('b');
        case ACT_ALPHA_C: return insertChar('c');
        case ACT_ALPHA_D: return insertChar('d');
        case ACT_ALPHA_E: return insertChar('e');
        case ACT_ALPHA_F: return insertChar('f');
            
        // Modes
        case ACT_FN1:
            currentMode = (currentMode == MODE_FN1) ? MODE_BASE : MODE_FN1;
            printf("Mode: %s\n", currentMode == MODE_FN1 ? "FN1" : "BASE");
            return true;
            
        case ACT_FN2:
            currentMode = (currentMode == MODE_FN2) ? MODE_BASE : MODE_FN2;
            printf("Mode: %s\n", currentMode == MODE_FN2 ? "FN2" : "BASE");
            return true;
            
        case ACT_MEM:
            currentMode = (currentMode == MODE_MEM) ? MODE_BASE : MODE_MEM;
            printf("Mode: %s\n", currentMode == MODE_MEM ? "MEM" : "BASE");
            return true;
            
        // Presets
        case ACT_PRESET_1:
            return preset_load(0, textBuffer);
        case ACT_PRESET_2:
            return preset_load(1, textBuffer);
        case ACT_PRESET_3:
            return preset_load(2, textBuffer);
        case ACT_PRESET_4:
            return preset_load(3, textBuffer);
        case ACT_PRESET_5:
            return preset_load(4, textBuffer);
        case ACT_PRESET_6:
            return preset_load(5, textBuffer);
        case ACT_PRESET_7:
            return preset_load(6, textBuffer);
        case ACT_PRESET_8:
            return preset_load(7, textBuffer);
        case ACT_PRESET_9:
            return preset_load(8, textBuffer);
            
        case ACT_PRESET_DEC:
            if (current_slot > 0) {
                current_slot--;
                return preset_load(current_slot, textBuffer);
            }
            return false;
            
        case ACT_PRESET_INC:
            if (current_slot < PRESET_COUNT - 1) {
                current_slot++;
                return preset_load(current_slot, textBuffer);
            }
            return false;
            
        case ACT_SAVE:
            return preset_save(current_slot, textBuffer);
            
        default:
            return false;
    }
}
