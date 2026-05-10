#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>
#include <stdbool.h>

// --- Button Mapping (Bitmasks) ---
// Adjust these if your wiring order is different!
#define BTN_SELECT (1 << 0)
#define BTN_START  (1 << 1)
#define BTN_B      (1 << 2)
#define BTN_A      (1 << 3)
#define BTN_R      (1 << 4)
#define BTN_D      (1 << 5)
#define BTN_U      (1 << 6)
#define BTN_L      (1 << 7)

// --- Function Prototypes ---

/**
 * Initialize GPIOs and start the 100Hz scanning timer.
 */
void buttons_init(int32_t scan_time);

/**
 * Returns the current raw state of all buttons.
 * (1 = Pressed, 0 = Released)
 */
uint8_t buttons_get_raw_state(void);

/**
 * Returns ONLY the buttons that were pressed since the last call.
 * Useful for triggering single events (like toggling a menu).
 */
uint8_t buttons_get_just_pressed(void);

char buttons_map_to_char_jukebox(uint8_t edge, int currentEq);

char get_button_jukebox(int currentEq);

char buttons_map_menu_navigation(void);

void buttons_sync_state(void);

#endif // BUTTONS_H