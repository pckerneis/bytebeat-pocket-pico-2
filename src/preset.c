#include "preset.h"
#include "ui.h"
#include "rpn_vm.h"
#include "display.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>

// Factory presets (same as Arduino version)
const char* const factoryPresets[PRESET_COUNT] = {
    "t*(42&t>>10)",
    "t*((t>>12)|(t>>8))",
    "t*(0xdeadbeef>>(t>>11)&15)/2|t>>3|t>>(t>>10)",
    "",
    "",
    "",
    "",
    "",
    ""
};

// Flash storage configuration
// Store presets in the last sector of flash (2MB - 4KB)
// Pico has 2MB flash, we'll use the last 4KB sector for presets
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define PRESET_FLASH_OFFSET(slot) (FLASH_TARGET_OFFSET + (slot) * PRESET_SLOT_SIZE)

// Marker to indicate empty slot
#define EMPTY_MARKER 0xFF

void preset_init(void) {
    printf("Preset system initialized (using flash storage)\n");
    printf("Flash offset: 0x%X\n", FLASH_TARGET_OFFSET);
}

void preset_clear_all(void) {
    uint32_t ints = save_and_disable_interrupts();
    
    // Erase the entire sector
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    
    restore_interrupts(ints);
    
    printf("All presets cleared from flash\n");
}

bool preset_is_slot_empty(uint8_t slot) {
    if (slot >= PRESET_COUNT) return true;
    
    // Read first byte from flash
    const uint8_t* flash_ptr = (const uint8_t*)(XIP_BASE + PRESET_FLASH_OFFSET(slot));
    return flash_ptr[0] == EMPTY_MARKER;
}

bool preset_load(uint8_t slot, char* exprBuffer) {
    if (slot >= PRESET_COUNT) return false;
    
    char msg[32];
    snprintf(msg, sizeof(msg), "Loaded P%d", slot + 1);
    show_toaster(msg);
    printf("%s\n", msg);
    
    if (preset_is_slot_empty(slot)) {
        // Load factory preset
        const char* factory = factoryPresets[slot];
        strcpy(exprBuffer, factory);
        
        text_len = strlen(exprBuffer);
        cursor = text_len;
        current_slot = slot;
        needsResetT = true;
        needsRecompile = true;
        
        printf("Loaded factory preset %d: %s\n", slot + 1, exprBuffer);
        return true;
    }
    
    // Load from flash
    const uint8_t* flash_ptr = (const uint8_t*)(XIP_BASE + PRESET_FLASH_OFFSET(slot));
    
    // Copy from flash to buffer
    for (int i = 0; i < TEXT_BUFFER_SIZE; i++) {
        char c = (char)flash_ptr[i];
        exprBuffer[i] = c;
        if (c == 0) {
            text_len = i;
            cursor = i;
            current_slot = slot;
            needsResetT = true;
            needsRecompile = true;
            break;
        }
    }
    
    printf("Loaded user preset %d: %s\n", slot + 1, exprBuffer);
    return true;
}

bool preset_save(uint8_t slot, const char* exprBuffer) {
    if (slot >= PRESET_COUNT) return false;
    
    char msg[32];
    snprintf(msg, sizeof(msg), "Saved P%d", slot + 1);
    show_toaster(msg);
    printf("%s\n", msg);
    
    // Prepare buffer for writing (must be FLASH_PAGE_SIZE aligned)
    uint8_t buffer[FLASH_SECTOR_SIZE];
    
    // Read entire sector first
    const uint8_t* flash_ptr = (const uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET);
    memcpy(buffer, flash_ptr, FLASH_SECTOR_SIZE);
    
    // Update the specific slot in the buffer
    uint8_t* slot_ptr = buffer + (slot * PRESET_SLOT_SIZE);
    memset(slot_ptr, EMPTY_MARKER, PRESET_SLOT_SIZE); // Clear slot first
    
    // Copy expression to slot
    size_t len = strlen(exprBuffer);
    if (len >= PRESET_SLOT_SIZE) len = PRESET_SLOT_SIZE - 1;
    memcpy(slot_ptr, exprBuffer, len);
    slot_ptr[len] = '\0';
    
    // Write back to flash
    uint32_t ints = save_and_disable_interrupts();
    
    // Erase sector
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    
    // Program sector
    flash_range_program(FLASH_TARGET_OFFSET, buffer, FLASH_SECTOR_SIZE);
    
    restore_interrupts(ints);
    
    current_slot = slot;
    printf("Saved preset %d to flash: %s\n", slot + 1, exprBuffer);
    return true;
}
