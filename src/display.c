#include "display.h"
#include "rpn_vm.h"
#include "ui.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

// I2C configuration
#define I2C_PORT i2c0
#define SDA_PIN 4
#define SCL_PIN 5

// Try both common OLED addresses
#define OLED_ADDR1 0x3C
#define OLED_ADDR2 0x3D
uint8_t OLED_ADDR = OLED_ADDR1;

// Display state
volatile bool oledDirty = false;
bool toasterVisible = false;
char toasterMsg[32] = {0};
uint32_t toasterStartTime = 0;

// Simple SSD1306 OLED driver functions
void oled_init(void);
void oled_clear(void);
void oled_set_pos(uint8_t x, uint8_t y);
void oled_write_data(uint8_t data);
void oled_write_cmd(uint8_t cmd);

void display_init(void) {
    printf("Initializing display on I2C0 pins SDA=%d, SCL=%d\n", SDA_PIN, SCL_PIN);
    
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);
    
    sleep_ms(100);
    
    printf("SDA pin function: %d (should be %d for I2C)\n", 
           gpio_get_function(SDA_PIN), GPIO_FUNC_I2C);
    printf("SCL pin function: %d (should be %d for I2C)\n", 
           gpio_get_function(SCL_PIN), GPIO_FUNC_I2C);
    
    bool oled_found = false;
    uint8_t rxdata;
    
    printf("Trying address 0x%02X...\n", OLED_ADDR1);
    int ret = i2c_read_blocking(I2C_PORT, OLED_ADDR1, &rxdata, 1, false);
    if (ret >= 0) {
        OLED_ADDR = OLED_ADDR1;
        oled_found = true;
        printf("OLED detected at 0x%02X\n", OLED_ADDR);
    } else {
        printf("No response from 0x%02X (error %d)\n", OLED_ADDR1, ret);
        printf("Trying address 0x%02X...\n", OLED_ADDR2);
        ret = i2c_read_blocking(I2C_PORT, OLED_ADDR2, &rxdata, 1, false);
        if (ret >= 0) {
            OLED_ADDR = OLED_ADDR2;
            oled_found = true;
            printf("OLED detected at 0x%02X\n", OLED_ADDR);
        } else {
            printf("No response from 0x%02X (error %d)\n", OLED_ADDR2, ret);
            printf("OLED not found - check wiring and power\n");
            OLED_ADDR = OLED_ADDR1;
        }
    }
    
    if (!oled_found) {
        printf("Skipping OLED initialization - no device found\n");
        return;
    }
    
    oled_init();
    oled_clear();
    
    oled_set_pos(0, 0);
    for (int i = 0; i < 16; i++) {
        oled_write_data(0xFF);
    }
    
    oledDirty = true;
    printf("Display initialization complete\n");
}

void oled_write_cmd(uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};
    i2c_write_blocking(I2C_PORT, OLED_ADDR, data, 2, false);
}

void oled_write_data(uint8_t data) {
    uint8_t buf[2] = {0x40, data};
    i2c_write_blocking(I2C_PORT, OLED_ADDR, buf, 2, false);
}

void oled_init(void) {
    sleep_ms(100);
    
    oled_write_cmd(0xAE); sleep_ms(10);
    oled_write_cmd(0xD5); oled_write_cmd(0x80); sleep_ms(10);
    oled_write_cmd(0xA8); oled_write_cmd(0x3F); sleep_ms(10);
    oled_write_cmd(0xD3); oled_write_cmd(0x00); sleep_ms(10);
    oled_write_cmd(0x40); sleep_ms(10);
    oled_write_cmd(0x8D); oled_write_cmd(0x14); sleep_ms(10);
    oled_write_cmd(0x20); oled_write_cmd(0x00); sleep_ms(10);
    oled_write_cmd(0xA1); sleep_ms(10);
    oled_write_cmd(0xC8); sleep_ms(10);
    oled_write_cmd(0xDA); oled_write_cmd(0x12); sleep_ms(10);
    oled_write_cmd(0x81); oled_write_cmd(0xCF); sleep_ms(10);
    oled_write_cmd(0xD9); oled_write_cmd(0xF1); sleep_ms(10);
    oled_write_cmd(0xDB); oled_write_cmd(0x40); sleep_ms(10);
    oled_write_cmd(0xA4); sleep_ms(10);
    oled_write_cmd(0xA6); sleep_ms(10);
    oled_write_cmd(0xAF); sleep_ms(100);
}

void oled_clear(void) {
    oled_write_cmd(0x20); oled_write_cmd(0x00);
    oled_write_cmd(0x21); oled_write_cmd(0x00); oled_write_cmd(0x7F);
    oled_write_cmd(0x22); oled_write_cmd(0x00); oled_write_cmd(0x07);
    
    for (int i = 0; i < 1024; i++) {
        oled_write_data(0x00);
    }
}

void oled_set_pos(uint8_t x, uint8_t y) {
    oled_write_cmd(0x20); oled_write_cmd(0x00);
    oled_write_cmd(0x21); oled_write_cmd(x); oled_write_cmd(0x7F);
    oled_write_cmd(0x22); oled_write_cmd(y); oled_write_cmd(0x07);
}

// 5x8 font for ASCII 32-127
const uint8_t font_5x8[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5F, 0x00, 0x00},
    {0x00, 0x07, 0x00, 0x07, 0x00}, {0x14, 0x7F, 0x14, 0x7F, 0x14},
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, {0x23, 0x13, 0x08, 0x64, 0x62},
    {0x36, 0x49, 0x55, 0x22, 0x50}, {0x00, 0x05, 0x03, 0x00, 0x00},
    {0x00, 0x1C, 0x22, 0x41, 0x00}, {0x00, 0x41, 0x22, 0x1C, 0x00},
    {0x14, 0x08, 0x3E, 0x08, 0x14}, {0x08, 0x08, 0x3E, 0x08, 0x08},
    {0x00, 0x50, 0x30, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08},
    {0x00, 0x60, 0x60, 0x00, 0x00}, {0x20, 0x10, 0x08, 0x04, 0x02},
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
    {0x00, 0x36, 0x36, 0x00, 0x00}, {0x00, 0x56, 0x36, 0x00, 0x00},
    {0x08, 0x14, 0x22, 0x41, 0x00}, {0x14, 0x14, 0x14, 0x14, 0x14},
    {0x00, 0x41, 0x22, 0x14, 0x08}, {0x02, 0x01, 0x51, 0x09, 0x06},
    {0x32, 0x49, 0x79, 0x41, 0x3E}, {0x7E, 0x11, 0x11, 0x11, 0x7E},
    {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x7F, 0x49, 0x49, 0x49, 0x41},
    {0x7F, 0x09, 0x09, 0x09, 0x01}, {0x3E, 0x41, 0x49, 0x49, 0x7A},
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41},
    {0x7F, 0x40, 0x40, 0x40, 0x40}, {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E},
    {0x7F, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7F, 0x01, 0x01}, {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x38, 0x40, 0x3F},
    {0x63, 0x14, 0x08, 0x14, 0x63}, {0x07, 0x08, 0x70, 0x08, 0x07},
    {0x61, 0x51, 0x49, 0x45, 0x43}, {0x00, 0x7F, 0x41, 0x41, 0x00},
    {0x02, 0x04, 0x08, 0x10, 0x20}, {0x00, 0x41, 0x41, 0x7F, 0x00},
    {0x04, 0x02, 0x01, 0x02, 0x04}, {0x40, 0x40, 0x40, 0x40, 0x40},
    {0x00, 0x01, 0x02, 0x04, 0x00}, {0x20, 0x54, 0x54, 0x54, 0x78},
    {0x7F, 0x48, 0x44, 0x44, 0x38}, {0x38, 0x44, 0x44, 0x44, 0x20},
    {0x38, 0x44, 0x44, 0x48, 0x7F}, {0x38, 0x54, 0x54, 0x54, 0x18},
    {0x08, 0x7E, 0x09, 0x01, 0x02}, {0x0C, 0x52, 0x52, 0x52, 0x3E},
    {0x7F, 0x08, 0x04, 0x04, 0x78}, {0x00, 0x44, 0x7D, 0x40, 0x00},
    {0x20, 0x40, 0x44, 0x3D, 0x00}, {0x7F, 0x10, 0x28, 0x44, 0x00},
    {0x00, 0x41, 0x7F, 0x40, 0x00}, {0x7C, 0x04, 0x18, 0x04, 0x78},
    {0x7C, 0x08, 0x04, 0x04, 0x78}, {0x38, 0x44, 0x44, 0x44, 0x38},
    {0x7C, 0x14, 0x14, 0x14, 0x08}, {0x08, 0x14, 0x14, 0x18, 0x7C},
    {0x7C, 0x08, 0x04, 0x04, 0x08}, {0x48, 0x54, 0x54, 0x54, 0x20},
    {0x04, 0x3F, 0x44, 0x40, 0x20}, {0x3C, 0x40, 0x40, 0x20, 0x7C},
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, {0x3C, 0x40, 0x30, 0x40, 0x3C},
    {0x44, 0x28, 0x10, 0x28, 0x44}, {0x0C, 0x50, 0x50, 0x50, 0x3C},
    {0x44, 0x64, 0x54, 0x4C, 0x44}, {0x00, 0x08, 0x36, 0x41, 0x00},
    {0x00, 0x00, 0x7F, 0x00, 0x00}, {0x00, 0x41, 0x36, 0x08, 0x00},
    {0x10, 0x08, 0x08, 0x10, 0x08}, {0x00, 0x00, 0x00, 0x00, 0x00}
};

void display_set_cursor(uint8_t x, uint8_t y) {
    oled_set_pos(x, y);
}

void display_print(const char* text) {
    while (*text) {
        char c = *text;
        if (c >= 32 && c <= 127) {
            uint8_t font_index = c - 32;
            for (int col = 0; col < 5; col++) {
                oled_write_data(font_5x8[font_index][col]);
            }
            oled_write_data(0x00);
        }
        text++;
    }
}

void display_clear() {
    oled_clear();
}

void show_toaster(const char* msg) {
    strncpy(toasterMsg, msg, sizeof(toasterMsg) - 1);
    toasterMsg[sizeof(toasterMsg) - 1] = '\0';
    toasterStartTime = to_ms_since_boot(get_absolute_time());
    toasterVisible = true;
    oledDirty = true;
}

void draw_error_banner(const char* error) {
    oled_set_pos(0, 5);
    for (int i = 0; i < 21; i++) {
        display_print(" ");
    }
    oled_set_pos(2, 5);
    display_print(error);
}

void draw_expression_editor(void) {
    // Draw header without clearing screen to avoid blink
    display_set_cursor(0, 0);
    char slotStr[8];
    sprintf(slotStr, "P%d", current_slot);
    display_print(slotStr);
    display_print("     "); // Clear rest of line
    
    display_set_cursor(50, 0);
    if (isPlaying) {
        display_print("PLAY");
    } else {
        display_print("STOP");
    }
    
    // Clear line 1 (separator)
    display_set_cursor(0, 1);
    for (int i = 0; i < 21; i++) {
        display_print(" ");
    }
    
    uint8_t line = 2;
    uint8_t col = 0;
    uint8_t charIndex = 0;
    
    // Draw expression text
    while (charIndex < text_len && line < 8) {
        char c = textBuffer[charIndex];
        
        if (col >= 21) {
            line++;
            col = 0;
            if (line >= 8) break;
        }
        
        display_set_cursor(col * 6, line);
        
        if (charIndex == cursor) {
            display_print("_");
        } else {
            char temp[2] = {c, '\0'};
            display_print(temp);
        }
        
        col++;
        charIndex++;
    }
    
    // Clear remaining characters on current line
    while (col < 21 && line < 8) {
        display_set_cursor(col * 6, line);
        display_print(" ");
        col++;
    }
    
    // Clear remaining lines
    line++;
    while (line < 5) { // Clear up to line 5 (before status area)
        display_set_cursor(0, line);
        for (int i = 0; i < 21; i++) {
            display_print(" ");
        }
        line++;
    }
    
    if (toasterVisible) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if ((now - toasterStartTime) > TOASTER_DURATION) {
            toasterVisible = false;
            oledDirty = true;
        } else {
            display_set_cursor(20, 5);
            display_print(toasterMsg);
        }
    } else if (compileError != ERR_NONE) {
        const char* errorMsg = "ERR: UNKNOWN";
        switch (compileError) {
            case ERR_PAREN: errorMsg = "ERR: PAREN"; break;
            case ERR_STACK: errorMsg = "ERR: STACK"; break;
            case ERR_TOKEN: errorMsg = "ERR: TOKEN"; break;
            case ERR_PROGRAM_TOO_LONG: errorMsg = "ERR: TOO LONG"; break;
            default: break;
        }
        draw_error_banner(errorMsg);
    }
}

void display_update(void) {
    if (oledDirty) {
        draw_expression_editor();
        oledDirty = false;
    }
}
