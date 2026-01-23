#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/sync.h"
#include "hardware/sync.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "audio.h"
#include "rpn_vm.h"
#include "ui.h"
#include "display.h"
#include "keyboard.h"
#include "preset.h"
#include "test_rpn.h"

#define SAMPLE_US (1000000 / 8000)
#define KEY_DEBOUNCE_MS 50
#define KEY_REPEAT_DELAY_MS 500  // Initial delay before repeat starts
#define KEY_REPEAT_RATE_MS 100   // Repeat rate once started

// Command buffer for serial input
#define CMD_BUFFER_SIZE 256

char cmd_buffer[CMD_BUFFER_SIZE];
uint8_t cmd_pos = 0;

// Atomic pointer swap for lock-free program updates
struct ProgramBuffer {
    struct RpnInstruction program[RPN_PROGRAM_SIZE];
    uint8_t length;
};

static struct ProgramBuffer program_buffers[2];
static volatile struct ProgramBuffer* active_program = &program_buffers[0];
volatile uint32_t t_audio = 0;

// I2C scanner for debugging
void i2c_scan(void) {
    printf("\n=== I2C Diagnostic ===\n");
    printf("I2C Port: i2c0\n");
    printf("SDA Pin: GPIO 4\n");
    printf("SCL Pin: GPIO 5\n");
    printf("Baudrate: 400kHz\n\n");
    
    printf("Scanning I2C bus (0x08 to 0x77)...\n");
    bool found_any = false;

    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        uint8_t data;
        int ret = i2c_read_blocking(i2c0, addr, &data, 1, false);

        if (ret >= 0) {
            printf("  [FOUND] Device at 0x%02X\n", addr);
            found_any = true;
        }
    }
    
    if (!found_any) {
        printf("  No I2C devices found!\n\n");
        printf("Troubleshooting steps:\n");
        printf("1. Check wiring:\n");
        printf("   - OLED SDA -> Pico GPIO 4\n");
        printf("   - OLED SCL -> Pico GPIO 5\n");
        printf("   - OLED VCC -> Pico 3.3V (pin 36)\n");
        printf("   - OLED GND -> Pico GND (pin 38)\n\n");
        printf("2. Verify OLED power:\n");
        printf("   - Check if OLED has power LED on\n");
        printf("   - Measure voltage at VCC pin (should be 3.3V)\n\n");
        printf("3. Try swapping SDA/SCL wires\n\n");
        printf("4. Try different GPIO pins:\n");
        printf("   - GPIO 0/1 (I2C0 alt)\n");
        printf("   - GPIO 6/7 (I2C1)\n");
        printf("   - GPIO 20/21 (I2C0 alt)\n\n");
        printf("5. Check OLED module:\n");
        printf("   - Some need 5V instead of 3.3V\n");
        printf("   - Verify it's SSD1306 compatible\n");
    }
    printf("\nI2C scan complete\n");
}

bool audio_cb(struct repeating_timer *t) {
    struct ProgramBuffer* prog = (struct ProgramBuffer*)
        __atomic_load_n(&active_program, __ATOMIC_ACQUIRE);

    uint32_t tval = t_audio;
    uint32_t result = executeRPN(tval, prog->program, prog->length);
    __atomic_store_n(&t_audio, tval + 1, __ATOMIC_RELAXED);

    int8_t s = (int8_t)((result & 0xFF) ^ 0x80);
    audio_write(s + 128);
    return true;
}

void process_command(char* cmd) {
    // Trim whitespace
    while (*cmd == ' ' || *cmd == '\t' || *cmd == '\n' || *cmd == '\r') cmd++;
    
    if (strlen(cmd) == 0) return;
    
    if (strcmp(cmd, "play") == 0 || strcmp(cmd, "start") == 0) {
        ui_handle_play_stop();
    } else if (strcmp(cmd, "stop") == 0) {
        if (isPlaying) {
            ui_handle_play_stop();
        }
    } else if (strcmp(cmd, "scan") == 0) {
        i2c_scan();
    } else if (strcmp(cmd, "test") == 0) {
        printf("Testing display...\n");
        display_clear();
        display_set_cursor(0, 0);
        display_print("TEST DISPLAY");
        display_set_cursor(0, 2);
        display_print("1234567890");
        display_set_cursor(0, 4);
        display_print("Pico Working!");
    } else if (strcmp(cmd, "pins") == 0) {
        printf("Trying alternative I2C pins...\n");
        // Try GPIO 20/21 (common alternative)
        printf("Trying GPIO 20 (SDA) / 21 (SCL)...\n");
        // Note: This would require recompilation with different pin definitions
        printf("To use different pins, edit display.c and change:\n");
        printf("  #define SDA_PIN 20\n");
        printf("  #define SCL_PIN 21\n");
        printf("Then rebuild and flash\n");
    } else if (strcmp(cmd, "init") == 0) {
        printf("Replaying display initialization...\n");
        display_init();
    } else if (strcmp(cmd, "slow") == 0) {
        printf("Trying slower I2C speed (100kHz)...\n");
        i2c_set_baudrate(i2c0, 100 * 1000);
        printf("I2C speed set to 100kHz\n");
        printf("Now run 'scan' to check for devices\n");
    } else if (strcmp(cmd, "fast") == 0) {
        printf("Setting I2C speed to 400kHz...\n");
        i2c_set_baudrate(i2c0, 400 * 1000);
        printf("I2C speed set to 400kHz\n");
    } else if (strcmp(cmd, "testall") == 0) {
        printf("Running all RPN VM tests...\n");
        runAllTests(0, 1000, false);
    } else if (strncmp(cmd, "testall ", 8) == 0) {
        uint32_t samples = atoi(cmd + 8);
        if (samples > 0 && samples <= 1000000) {
            printf("Running all RPN VM tests with %lu samples...\n", (unsigned long)samples);
            runAllTests(0, samples, true);
        } else {
            printf("Invalid sample count. Use 1-1000000\n");
        }
    } else if (strncmp(cmd, "testcase ", 9) == 0) {
        int testIndex = atoi(cmd + 9);
        printf("Running test case %d...\n", testIndex);
        runSingleTest(testIndex, 0, 10000, true);
    } else if (strcmp(cmd, "testlist") == 0) {
        listTests();
    } else if (strcmp(cmd, "help") == 0) {
        printf("Commands:\n");
        printf("  play/start - Start audio playback\n");
        printf("  stop       - Stop audio playback\n");
        printf("  expr <...> - Set bytebeat expression\n");
        printf("  load <n>   - Load preset 1-9\n");
        printf("  save <n>   - Save current expression to preset 1-9\n");
        printf("  clear      - Clear all presets\n");
        printf("  scan       - Scan I2C bus for devices\n");
        printf("  test       - Test display output\n");
        printf("  testall [n]- Run all RPN VM unit tests (optional: n samples)\n");
        printf("  testcase n - Run specific test case by index\n");
        printf("  testlist   - List all available test cases\n");
        printf("  slow       - Set I2C to 100kHz (for troubleshooting)\n");
        printf("  fast       - Set I2C to 400kHz (default)\n");
        printf("  pins       - Show alternative pin options\n");
        printf("  init       - Replay display initialization\n");
        printf("  help       - Show this help\n");
        printf("Examples:\n");
        printf("  expr t*(42&t>>10)\n");
        printf("  expr t*((t>>12)|(t>>8))\n");
        printf("  expr t*(0xdeadbeef>>(t>>11)&15)/2|t>>3|t>>(t>>10)\n");
        printf("  load 1\n");
        printf("  save 3\n");
        printf("  testall 5000\n");
        printf("  testcase 0\n");
    } else if (strncmp(cmd, "load ", 5) == 0) {
        int slot = atoi(cmd + 5);
        if (slot >= 1 && slot <= PRESET_COUNT) {
            preset_load(slot - 1, textBuffer);
            needsRecompile = true;
            oledDirty = true;
        } else {
            printf("Invalid preset slot. Use 1-%d\n", PRESET_COUNT);
        }
    } else if (strncmp(cmd, "save ", 5) == 0) {
        int slot = atoi(cmd + 5);
        if (slot >= 1 && slot <= PRESET_COUNT) {
            preset_save(slot - 1, textBuffer);
        } else {
            printf("Invalid preset slot. Use 1-%d\n", PRESET_COUNT);
        }
    } else if (strcmp(cmd, "clear") == 0) {
        printf("Clearing all presets...\n");
        preset_clear_all();
        printf("All presets cleared.\n");
    } else if (strncmp(cmd, "expr ", 5) == 0) {
        char* expr = cmd + 5;
        ui_set_expression(expr);
    } else {
        printf("Unknown command: %s\n", cmd);
        printf("Type 'help' for available commands\n");
    }
}

void check_serial_input() {
    if (stdio_usb_connected()) {
        int c = getchar_timeout_us(0);
        if (c >= 0) {
            if (c == '\n' || c == '\r') {
                if (cmd_pos > 0) {
                    cmd_buffer[cmd_pos] = '\0';
                    process_command(cmd_buffer);
                    cmd_pos = 0;
                    printf("> ");
                }
            } else if (c == 8 || c == 127) { // Backspace
                if (cmd_pos > 0) {
                    cmd_pos--;
                    printf("\b \b");
                }
            } else if (cmd_pos < CMD_BUFFER_SIZE - 1) {
                cmd_buffer[cmd_pos++] = (char)c;
                putchar(c);
            }
        }
    }
}

void core1_main() {
    printf("\n=== Bytebeat Pocket for Raspberry Pico ===\n");
    printf("RPN VM Compiler and Audio System Ported\n");
    printf("Keyboard matrix enabled\n");
    printf("Type 'help' for commands\n");
    printf("Type 'init' to replay initialization messages\n");
    printf("> ");

    static uint8_t lastProcessedKey = 255;
    static uint8_t currentHeldKey = 255;
    static uint32_t keyPressTime = 0;
    static uint32_t lastRepeatTime = 0;
    static bool repeatStarted = false;

    while (true) {
        check_serial_input();
        
        // Scan keyboard matrix
        keyboard_scan();
        
        // Get currently pressed key
        uint8_t k = keyboard_get_pressed_key();
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        if (k != 255) {
            // Key is pressed
            if (k != currentHeldKey) {
                // New key pressed
                Action action = keyboard_resolve_action(k);
                if (keyboard_execute_action(action)) {
                    oledDirty = true;
                }
                currentHeldKey = k;
                keyPressTime = now;
                lastRepeatTime = now;
                repeatStarted = false;
            } else {
                // Same key held down - check for repeat
                if (!repeatStarted) {
                    // Check if initial delay has passed
                    if ((now - keyPressTime) >= KEY_REPEAT_DELAY_MS) {
                        repeatStarted = true;
                        lastRepeatTime = now;
                    }
                } else {
                    // Repeat is active - check repeat rate
                    if ((now - lastRepeatTime) >= KEY_REPEAT_RATE_MS) {
                        Action action = keyboard_resolve_action(k);
                        if (keyboard_execute_action(action)) {
                            oledDirty = true;
                        }
                        lastRepeatTime = now;
                    }
                }
            }
        } else {
            // No key pressed - reset state
            currentHeldKey = 255;
            repeatStarted = false;
        }
        
        // Handle recompilation when key is released
        if (k == 255 && needsRecompile) {
            // Determine which buffer is inactive
            struct ProgramBuffer* current = (struct ProgramBuffer*)active_program;
            struct ProgramBuffer* next = (current == &program_buffers[0]) ? 
                                          &program_buffers[1] : &program_buffers[0];
            
            // Compile to RPN
            uint8_t len = compileToRPN(next->program);
            next->length = (compileError == ERR_NONE) ? len : 0;
            
            if (needsResetT) {
                t_audio = 0;
                needsResetT = false;
            }
            
            // Atomic pointer swap
            __atomic_store_n(&active_program, next, __ATOMIC_RELEASE);
            
            needsRecompile = false;
            oledDirty = true;
        }
        
        ui_update();
        tight_loop_contents();
    }
}

int main() {
    // Initialize program buffers
    program_buffers[0].length = 0;
    program_buffers[1].length = 0;

    set_sys_clock_khz(125000, true);
    stdio_init_all();
    
    // Wait for serial connection
    sleep_ms(3000);
    
    audio_init();
    
    // Run RPN VM tests
    test_rpn_vm();
    
    keyboard_init();
    preset_init();
    
    // Check if MEM key is pressed on boot to clear all presets
    keyboard_scan();
    if (keyboard_is_key_pressed(KEY_MEM)) {
        printf("\n=== CLEARING ALL PRESETS ===\n");
        preset_clear_all();
        
        // Show confirmation on display
        display_clear();
        display_set_cursor(0, 2);
        display_print("All presets");
        display_set_cursor(0, 4);
        display_print("cleared!");
        sleep_ms(2000);
        
        // Wait for key release
        while (keyboard_is_key_pressed(KEY_MEM)) {
            keyboard_scan();
            sleep_ms(50);
        }
        
        printf("Presets cleared. Continuing boot...\n");
    }
    
    ui_init();

    preset_load(0, textBuffer);
    
    // Compile initial expression
    if (needsRecompile) {
        uint8_t len = compileToRPN(program_buffers[0].program);
        program_buffers[0].length = (compileError == ERR_NONE) ? len : 0;
        if (needsResetT) {
            t_audio = 0;
            needsResetT = false;
        }
        needsRecompile = false;
        printf("Initial expression compiled, length: %d\n", program_buffers[0].length);
    }

    static struct repeating_timer timer;
    add_repeating_timer_us(-SAMPLE_US, audio_cb, NULL, &timer);

    multicore_launch_core1(core1_main);
    
    while (true) {
        tight_loop_contents();
    }
}
