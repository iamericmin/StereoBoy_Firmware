#include "lib/sb_util/global_vars.h"
#include "buttons.h"

#define HOLD_TIME 50

const uint PIN_LATCH = 11;
const uint PIN_CLOCK = 23;
const uint PIN_DATA  = 22;

volatile uint8_t current_button_states = 0;
uint8_t last_button_states = 0; // Used for edge detection

static bool reading_timer_callback(struct repeating_timer *t) {
    uint8_t reading = 0;

    // Pulse SH/LD pin of shift register to capture all 8 button state
    gpio_put(PIN_LATCH, 0);
    busy_wait_us_32(1);
    gpio_put(PIN_LATCH, 1);

    // Clock shift register 8 times to read in all 8 button states
    for(int i = 0; i < 8; i++) {
        // read shift register's serial output pin
        if (!gpio_get(PIN_DATA)) {
            reading |= (1 << (7 - i));
        }
        
        // pulse SR clock
        gpio_put(PIN_CLOCK, 1);
        busy_wait_us_32(1);
        gpio_put(PIN_CLOCK, 0);
    }

    current_button_states = reading;
    return true;
}

// --- Public Functions ---

void buttons_init(int32_t scan_time) {
    // GPIO Init
    gpio_init(PIN_LATCH); gpio_set_dir(PIN_LATCH, GPIO_OUT); gpio_put(PIN_LATCH, 1);
    gpio_init(PIN_CLOCK); gpio_set_dir(PIN_CLOCK, GPIO_OUT); gpio_put(PIN_CLOCK, 0);
    gpio_init(PIN_DATA);  gpio_set_dir(PIN_DATA, GPIO_IN);
    
    buttons_sync_state();
    // We use a static variable for the timer struct so it persists
    static struct repeating_timer timer;
    add_repeating_timer_ms(scan_time, reading_timer_callback, NULL, &timer);
}

uint8_t buttons_get_raw_state(void) {
    return current_button_states;
}

uint8_t buttons_get_just_pressed(void) {
    // Snapshot current state
    uint8_t current = current_button_states;
    
    // Detect rising edges (0 -> 1)
    uint8_t just_pressed = (current ^ last_button_states) & current;
    
    // Update history
    last_button_states = current;
    
    return just_pressed;
}


char buttons_map_to_char_jukebox(uint8_t edge, int currentEq) {
    bool select_held = (~buttons_get_raw_state() & BTN_SELECT);

    if (!select_held) {
        if (edge & BTN_A)     return 'p';
        if (edge & BTN_B)     return 's';
        if (edge & BTN_U)     return 'u';
        if (edge & BTN_D)     return 'd';
        if (edge & BTN_R)     return 'n';
        if (edge & BTN_L)     return 'o';
        if (edge & BTN_START) return 'v';
    } else {
        // Fast Forward and Rewind are handled by the repeat logic above,
        // but you can keep them here as a fallback for the first click.
        if (edge & BTN_U)     return '+';
        if (edge & BTN_D)     return '-';
        if (edge & BTN_B)     return 'L';
        if (edge & BTN_A)     return (char)(((currentEq + 1) % 6) + '0');
    }
    return 0;
}

/**
 * 2. FOR MAIN MENU: Maps buttons to navigation
 * Returns: 'U'(Up), 'D'(Down), 'L'(-5), 'R'(+5), 'E'(Enter/Start)
 */
char buttons_map_menu_navigation(void) {
    // uint8_t edge = buttons_get_just_pressed();
    // if (edge == 0) return 0;
    if (BTN_U)     return 'U';
    if (BTN_D)     return 'D';
    if (BTN_L)     return 'L';
    if (BTN_R)     return 'R';
    if (BTN_START) return 'E';
    return 0;
}

void buttons_sync_state(void) {
    // 1. Force a manual read of the shift register right now
    reading_timer_callback(NULL); 
    
    // 2. Fast-forward the history so no "edges" are detected
    // from whatever buttons are currently being held down.
    last_button_states = current_button_states;
}

static uint32_t next_repeat_time = 0;

char get_button_jukebox(int currentEq) {
    uint8_t raw = ~buttons_get_raw_state(); // Current physical state
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // 1. CHECK FOR HELD REPEAT (Fast Forward / Rewind)
    // Only triggers if SELECT is held and either LEFT or RIGHT is held
    if (raw & BTN_SELECT) {
        if ((raw & BTN_R) || (raw & BTN_L)) {
            if (now >= next_repeat_time) {
                next_repeat_time = now + HOLD_TIME;
                return (raw & BTN_R) ? 'f' : 'r';
            }
            return 0; // Still waiting for the next "tick"
        }
    }

    // 2. CHECK FOR SINGLE PRESSES (Everything else)
    // We call this to get buttons that were JUST clicked
    uint8_t edge = buttons_get_just_pressed();
    if (edge == 0) return 0;

    // Reuse your existing mapping logic but pass the 'edge' to it
    return buttons_map_to_char_jukebox(edge, currentEq);
}
