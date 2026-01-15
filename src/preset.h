#pragma once
#include <stdint.h>
#include <stdbool.h>

#define PRESET_COUNT 9
#define PRESET_SLOT_SIZE 256

// Factory presets
extern const char* const factoryPresets[PRESET_COUNT];

// Initialize preset system (flash storage)
void preset_init(void);

// Clear all presets in flash
void preset_clear_all(void);

// Check if a slot is empty
bool preset_is_slot_empty(uint8_t slot);

// Load preset from slot (returns false if slot invalid)
bool preset_load(uint8_t slot, char* exprBuffer);

// Save preset to slot (returns false if slot invalid)
bool preset_save(uint8_t slot, const char* exprBuffer);
