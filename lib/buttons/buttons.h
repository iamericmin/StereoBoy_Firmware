#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>
#include <stdbool.h>

// Button Mapping
#define BTN_SELECT 0b11111110
#define BTN_START  0b11111101
#define BTN_B      0b11111011
#define BTN_A      0b11110111
#define BTN_R      0b11101111
#define BTN_D      0b11011111
#define BTN_U      0b10111111
#define BTN_L      0b01111111

void buttons_init(int32_t scan_time);

uint8_t buttons_get_raw_state(void);

// Returns ONLY the buttons that were pressed since the last call.
// Useful for triggering single events (like toggling a menu).
uint8_t buttons_get_just_pressed(void);

uint8_t buttons_read_long_press();

#endif // BUTTONS_H