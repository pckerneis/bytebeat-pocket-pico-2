#include "display.h"
#include "rpn_vm.h"
#include "ui.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

// SPI configuration for Waveshare 2inch LCD Module (ST7789, 240x320)
#define LCD_SPI spi1  // Using SPI1 because GPIO 10/11 are SPI1 pins
#define LCD_SPI_BAUDRATE (32 * 1000 * 1000) // 32 MHz (can go up to 62.5MHz)

// Pin definitions for Waveshare 2inch LCD Module
// GPIO 10/11 are SPI1 pins (SCK and TX respectively)
#define LCD_PIN_SCK  10  // SPI1 SCK  (CLK/SCL)
#define LCD_PIN_MOSI 11  // SPI1 TX   (DIN/SDA/MOSI)
#define LCD_PIN_DC   8   // Data/Command (DC)
#define LCD_PIN_CS   9   // Chip Select (CS)
#define LCD_PIN_RST  12  // Reset (RST)
#define LCD_PIN_BL   13  // Backlight (BL)

#define CHAR_W 11
#define CHAR_H 16

// Display state
volatile bool oledDirty = false;
bool toasterVisible = false;
char toasterMsg[32] = {0};
uint32_t toasterStartTime = 0;

// Frame buffer for text rendering (optional - can render directly)
static uint16_t framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT];
static bool use_framebuffer = false; // Set to true if you want double buffering

// ST7789 Commands
#define ST7789_NOP     0x00
#define ST7789_SWRESET 0x01
#define ST7789_SLPOUT  0x11
#define ST7789_NORON   0x13
#define ST7789_INVOFF  0x20
#define ST7789_INVON   0x21
#define ST7789_DISPOFF 0x28
#define ST7789_DISPON  0x29
#define ST7789_CASET   0x2A
#define ST7789_RASET   0x2B
#define ST7789_RAMWR   0x2C
#define ST7789_MADCTL  0xA0
#define ST7789_COLMOD  0x3A

// RGB565 color definitions
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_GRAY    0x8410

// Current colors
static uint16_t fg_color = COLOR_WHITE;
static uint16_t bg_color = COLOR_BLACK;

// LCD control functions
static inline void lcd_dc_low(void) { gpio_put(LCD_PIN_DC, 0); }
static inline void lcd_dc_high(void) { gpio_put(LCD_PIN_DC, 1); }
static inline void lcd_cs_low(void) { gpio_put(LCD_PIN_CS, 0); }
static inline void lcd_cs_high(void) { gpio_put(LCD_PIN_CS, 1); }
static inline void lcd_rst_low(void) { gpio_put(LCD_PIN_RST, 0); }
static inline void lcd_rst_high(void) { gpio_put(LCD_PIN_RST, 1); }

static void lcd_write_cmd(uint8_t cmd) {
    // Arduino: pull CS low, send command, leave CS low
    lcd_cs_low();
    lcd_dc_low();
    spi_write_blocking(LCD_SPI, &cmd, 1);
    // CS stays LOW - will be managed by subsequent data writes
}

static void lcd_write_data(uint8_t data) {
    // Arduino: CS should already be low from command, set DC high, send data, raise CS
    // But to be safe, ensure CS is low first
    lcd_cs_low();
    lcd_dc_high();
    spi_write_blocking(LCD_SPI, &data, 1);
    lcd_cs_high();
    // Each data byte gets its own CS cycle
}

static void lcd_write_data_buf(const uint8_t* buf, size_t len) {
    lcd_dc_high();
    lcd_cs_low();
    spi_write_blocking(LCD_SPI, buf, len);
    lcd_cs_high();
}

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // CASET - Column Address Set (send as 16-bit: high byte, low byte)
    lcd_write_cmd(ST7789_CASET);
    lcd_write_data(x0 >> 8);        // x0 high byte
    lcd_write_data(x0 & 0xFF);      // x0 low byte
    lcd_write_data((x1 - 1) >> 8);  // x1 high byte
    lcd_write_data((x1 - 1) & 0xFF);// x1 low byte
    
    // RASET - Row Address Set (send as 16-bit: high byte, low byte)
    lcd_write_cmd(ST7789_RASET);
    lcd_write_data(y0 >> 8);        // y0 high byte
    lcd_write_data(y0 & 0xFF);      // y0 low byte
    lcd_write_data((y1 - 1) >> 8);  // y1 high byte
    lcd_write_data((y1 - 1) & 0xFF);// y1 low byte
    
    // RAMWR - Memory Write command (CS must stay LOW after this for pixel data)
    // Don't use lcd_write_cmd here because we need CS to stay low
    lcd_cs_low();
    lcd_dc_low();
    spi_write_blocking(LCD_SPI, (uint8_t[]){ST7789_RAMWR}, 1);
    // CS stays LOW - caller will write pixel data then raise CS
}

static void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;
    if (x + w > SCREEN_WIDTH) w = SCREEN_WIDTH - x;
    if (y + h > SCREEN_HEIGHT) h = SCREEN_HEIGHT - y;
    
    uint32_t pixel_count = w * h;
    
    // Set window (RAMWR command leaves CS low)
    lcd_set_window(x, y, x + w, y + h);
    
    // Write pixel data - CS is low from RAMWR, set DC high and write all pixels
    lcd_dc_high();
    uint8_t color_buf[2] = {color >> 8, color & 0xFF};
    for (uint32_t i = 0; i < pixel_count; i++) {
        spi_write_blocking(LCD_SPI, color_buf, 2);
    }
    // Raise CS to complete the transaction
    lcd_cs_high();
}

// Forward declarations
void display_draw_char(char c, uint16_t x, uint16_t y, uint16_t fg, uint16_t bg);
void display_clear(void);

void display_init(void) {
    printf("\n=== LCD INITIALIZATION ===\n");
    printf("Waveshare 2inch LCD (ST7789, 240x320)\n");
    printf("Pin Configuration:\n");
    printf("  SCK  (GPIO %d) - SPI0 Clock\n", LCD_PIN_SCK);
    printf("  MOSI (GPIO %d) - SPI0 Data\n", LCD_PIN_MOSI);
    printf("  DC   (GPIO %d) - Data/Command\n", LCD_PIN_DC);
    printf("  CS   (GPIO %d) - Chip Select\n", LCD_PIN_CS);
    printf("  RST  (GPIO %d) - Reset\n", LCD_PIN_RST);
    printf("  BL   (GPIO %d) - Backlight\n", LCD_PIN_BL);
    
    // Initialize SPI with Mode 0 (CPOL=0, CPHA=0) as per Waveshare 2inch LCD specs
    printf("\nInitializing SPI0 at %d Hz...\n", LCD_SPI_BAUDRATE);
    uint actual_baud = spi_init(LCD_SPI, LCD_SPI_BAUDRATE);
    printf("  Actual baud rate: %d Hz\n", actual_baud);
    
    spi_set_format(LCD_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    printf("  SPI format: 8-bit, Mode 0 (CPOL=0, CPHA=0), MSB first\n");
    
    gpio_set_function(LCD_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(LCD_PIN_MOSI, GPIO_FUNC_SPI);
    printf("  SPI pins configured\n");
    
    // Initialize control pins
    printf("\nInitializing control pins...\n");
    gpio_init(LCD_PIN_DC);
    gpio_set_dir(LCD_PIN_DC, GPIO_OUT);
    gpio_put(LCD_PIN_DC, 1); // Default high
    printf("  DC pin initialized (high)\n");
    
    gpio_init(LCD_PIN_CS);
    gpio_set_dir(LCD_PIN_CS, GPIO_OUT);
    gpio_put(LCD_PIN_CS, 1); // Default high (inactive)
    printf("  CS pin initialized (high/inactive)\n");
    
    gpio_init(LCD_PIN_RST);
    gpio_set_dir(LCD_PIN_RST, GPIO_OUT);
    gpio_put(LCD_PIN_RST, 1); // Default high
    printf("  RST pin initialized (high)\n");
    
    // Initialize backlight
    gpio_init(LCD_PIN_BL);
    gpio_set_dir(LCD_PIN_BL, GPIO_OUT);
    gpio_put(LCD_PIN_BL, 1); // Turn on backlight
    printf("  Backlight ON\n");
    
    // Hardware reset (matching Arduino timing: 200ms delays)
    printf("\nPerforming hardware reset...\n");
    sleep_ms(200);
    lcd_rst_low();
    printf("  RST=LOW (reset active)\n");
    sleep_ms(200);
    lcd_rst_high();
    printf("  RST=HIGH (reset released)\n");
    sleep_ms(200);
    printf("  Reset complete\n");
    
    // ST7789 initialization sequence (from Waveshare LCD_2inch.c)
    printf("\nSending ST7789 initialization commands (Waveshare sequence)...\n");
    
    lcd_write_cmd(0x36); // MADCTL
    lcd_write_data(ST7789_MADCTL); // Try 0x70 for landscape orientation
    
    lcd_write_cmd(0x3A); // COLMOD
    lcd_write_data(0x05); // 16-bit color
    
    lcd_write_cmd(0x21); // INVON
    
    lcd_write_cmd(0x2A); // CASET - Column Address Set
    lcd_write_data(0x00);
    lcd_write_data(0x00); // Start column 0
    lcd_write_data(0x01);
    lcd_write_data(0x3F); // End column 319 (0x013F)
    
    lcd_write_cmd(0x2B); // RASET - Row Address Set
    lcd_write_data(0x00);
    lcd_write_data(0x00); // Start row 0
    lcd_write_data(0x00);
    lcd_write_data(0xEF); // End row 239 (0x00EF)
    
    lcd_write_cmd(0xB2); // PORCTRL
    lcd_write_data(0x0C);
    lcd_write_data(0x0C);
    lcd_write_data(0x00);
    lcd_write_data(0x33);
    lcd_write_data(0x33);
    
    lcd_write_cmd(0xB7); // GCTRL
    lcd_write_data(0x35);
    
    lcd_write_cmd(0xBB); // VCOMS
    lcd_write_data(0x1F);
    
    lcd_write_cmd(0xC0); // LCMCTRL
    lcd_write_data(0x2C);
    
    lcd_write_cmd(0xC2); // VDVVRHEN
    lcd_write_data(0x01);
    
    lcd_write_cmd(0xC3); // VRHS
    lcd_write_data(0x12);
    
    lcd_write_cmd(0xC4); // VDVS
    lcd_write_data(0x20);
    
    lcd_write_cmd(0xC6); // FRCTRL2
    lcd_write_data(0x0F);
    
    lcd_write_cmd(0xD0); // PWCTRL1
    lcd_write_data(0xA4);
    lcd_write_data(0xA1);
    
    lcd_write_cmd(0xE0); // PVGAMCTRL
    lcd_write_data(0xD0);
    lcd_write_data(0x08);
    lcd_write_data(0x11);
    lcd_write_data(0x08);
    lcd_write_data(0x0C);
    lcd_write_data(0x15);
    lcd_write_data(0x39);
    lcd_write_data(0x33);
    lcd_write_data(0x50);
    lcd_write_data(0x36);
    lcd_write_data(0x13);
    lcd_write_data(0x14);
    lcd_write_data(0x29);
    lcd_write_data(0x2D);
    
    lcd_write_cmd(0xE1); // NVGAMCTRL
    lcd_write_data(0xD0);
    lcd_write_data(0x08);
    lcd_write_data(0x10);
    lcd_write_data(0x08);
    lcd_write_data(0x06);
    lcd_write_data(0x06);
    lcd_write_data(0x39);
    lcd_write_data(0x44);
    lcd_write_data(0x51);
    lcd_write_data(0x0B);
    lcd_write_data(0x16);
    lcd_write_data(0x14);
    lcd_write_data(0x2F);
    lcd_write_data(0x31);
    
    lcd_write_cmd(0x21); // INVON
    
    lcd_write_cmd(0x11); // SLPOUT
    sleep_ms(120);
    
    lcd_write_cmd(0x29); // DISPON
    sleep_ms(20);
    
    printf("LCD commands sent, testing display...\n");
    
    // Test backlight control to verify GPIO is working
    printf("Testing backlight (should blink)...\n");
    for (int i = 0; i < 3; i++) {
        gpio_put(LCD_PIN_BL, 0);
        sleep_ms(200);
        gpio_put(LCD_PIN_BL, 1);
        sleep_ms(200);
    }
    printf("Backlight test complete\n");
    
    // Clear screen to black
    printf("\n*** Clearing screen to black ***\n");
    lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
    printf("Screen cleared\n");
    sleep_ms(200);
    
    // Draw test pattern using our working lcd_fill_rect
    printf("\n*** Drawing test pattern ***\n");
    lcd_fill_rect(10, 10, 100, 50, COLOR_RED);
    lcd_fill_rect(10, 70, 100, 50, COLOR_GREEN);
    lcd_fill_rect(10, 130, 100, 50, COLOR_BLUE);
    printf("Test pattern complete - should see red, green, blue rectangles\n");
    
    // Test text rendering with a simple pattern
    printf("\n*** Testing text rendering ***\n");
    fg_color = COLOR_WHITE;
    bg_color = COLOR_BLACK;
    display_set_cursor(120, 10);
    display_print("LCD OK!");
    printf("Text test complete - should see 'LCD OK!'\n");
    
    // Don't set oledDirty yet - let the test pattern stay visible
    oledDirty = false;
    printf("LCD initialization complete\n");
    printf("Test pattern will remain for 5 seconds...\n");
    sleep_ms(5000);
    
    // Now allow UI to draw
    oledDirty = true;
}

void display_clear(void) {
    lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BLACK);
}

#include "font16_data.h"


static uint16_t cursor_x = 0;
static uint16_t cursor_y = 0;

void display_set_cursor(uint16_t x, uint16_t y) {
    cursor_x = x;
    cursor_y = y;
}

void display_draw_char(char c, uint16_t x, uint16_t y, uint16_t fg, uint16_t bg) {
    if (c < 32 || c > 127) c = ' ';

    const int char_width = 11;
    const int char_height = 16;
    
    const uint8_t* font_data = &Font16_Table[(c - 32) * 32];
    
    lcd_set_window(x, y, x + char_width, y + char_height);
    lcd_dc_high();
    uint8_t bg_bytes[2] = {bg >> 8, bg & 0xFF};
    for (int i = 0; i < char_width * char_height; i++) {
        spi_write_blocking(LCD_SPI, bg_bytes, 2);
    }
    lcd_cs_high();
    
    for (int row = 0; row < char_height; row++) {
        uint16_t row_data = (font_data[row * 2] << 8) | font_data[row * 2 + 1];
        
        for (int col = 0; col < char_width; col++) {
            bool pixel = (row_data & (0x8000 >> col)) != 0;
            if (pixel) {
                lcd_set_window(x + col, y + row, x + col + 1, y + row + 1);
                lcd_dc_high();
                uint8_t fg_bytes[2] = {fg >> 8, fg & 0xFF};
                spi_write_blocking(LCD_SPI, fg_bytes, 2);
                lcd_cs_high();
            }
        }
    }
}

void display_print(const char* text) {
    const int char_width = CHAR_W;
    const int char_height = CHAR_H;
    
    while (*text) {
        if (*text == '\n') {
            cursor_x = 0;
            cursor_y += char_height;
        } else {
            display_draw_char(*text, cursor_x, cursor_y, fg_color, bg_color);
            cursor_x += char_width;
            
            if (cursor_x >= SCREEN_WIDTH) {
                cursor_x = 0;
                cursor_y += char_height;
            }
        }
        text++;
    }
}

void show_toaster(const char* msg) {
    strncpy(toasterMsg, msg, sizeof(toasterMsg) - 1);
    toasterMsg[sizeof(toasterMsg) - 1] = '\0';
    toasterStartTime = to_ms_since_boot(get_absolute_time());
    toasterVisible = true;
    oledDirty = true;
}

void draw_error_banner(const char* error) {
    // Draw error at bottom of screen
    lcd_fill_rect(0, SCREEN_HEIGHT - 20, SCREEN_WIDTH, 20, COLOR_RED);
    fg_color = COLOR_WHITE;
    display_set_cursor(10, SCREEN_HEIGHT - 16);
    display_print(error);
    fg_color = COLOR_WHITE;
}

void draw_expression_editor(void) {
    // Clear screen
    display_clear();
    
    // Draw header bar
    lcd_fill_rect(0, 0, SCREEN_WIDTH, 24, COLOR_BLUE);
    fg_color = COLOR_WHITE;
    bg_color = COLOR_BLUE;
    
    display_set_cursor(10, 6);
    char slotStr[16];
    sprintf(slotStr, "Preset %d", current_slot + 1);
    display_print(slotStr);
    
    display_set_cursor(SCREEN_WIDTH - 56, 6);
    if (isPlaying) {
        display_print("PLAY");
    } else {
        display_print("STOP");
    }
    
    // Reset cursor position to prevent it from affecting expression drawing
    cursor_x = 0;
    cursor_y = 0;
    
    // Reset colors for main text area
    fg_color = COLOR_WHITE;
    bg_color = COLOR_BLACK;
    
    // Draw expression text
    const int char_width = CHAR_W;
    const int char_height = CHAR_H;
    const int start_y = 30;
    
    uint16_t x = 0;
    uint16_t y = start_y;
    
    for (uint8_t i = 0; i < text_len && y < SCREEN_HEIGHT - 40; i++) {
        if (x >= SCREEN_WIDTH - char_width) {
            x = 0;
            y += char_height;
        }
        
        // Highlight cursor position - show character in yellow when over text
        if (i == cursor) {
            display_draw_char(textBuffer[i], x, y, COLOR_YELLOW, bg_color);
        } else {
            display_draw_char(textBuffer[i], x, y, fg_color, bg_color);
        }
        
        x += char_width;
    }
    
    // Draw underscore cursor only at end of text
    if (cursor == text_len) {
        display_draw_char('_', x, y, COLOR_YELLOW, bg_color);
    }
    
    // Draw toaster or error at bottom
    if (toasterVisible) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if ((now - toasterStartTime) > TOASTER_DURATION) {
            toasterVisible = false;
            oledDirty = true;
        } else {
            lcd_fill_rect(0, SCREEN_HEIGHT - 24, SCREEN_WIDTH, 24, COLOR_GREEN);
            fg_color = COLOR_BLACK;
            bg_color = COLOR_GREEN;
            display_set_cursor(10, SCREEN_HEIGHT - 18);
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
