#include "lib/sb_util/global_vars.h"
#include "buttons.h"

// --- Pin Definitions ---
static const uint PIN_LATCH = 11;
static const uint PIN_CLOCK = 23;
static const uint PIN_DATA  = 22;

// --- Internal State ---
static volatile uint8_t current_button_states = 0;
static uint8_t last_button_states = 0; // Used for edge detection

// --- Timer Interrupt Callback ---
static bool reading_timer_callback(struct repeating_timer *t) {
    uint8_t reading = 0;

    // 1. latch values (Pulse High-Low-High)
    gpio_put(PIN_LATCH, 0);
    busy_wait_us_32(1);
    gpio_put(PIN_LATCH, 1);

    // 2. shift ivalues
    for(int i = 0; i < 8; i++) {
        // Might need to invert for Active low buttons (change: to !gpio_get(PIN_DATA))
        if (!gpio_get(PIN_DATA)) {
            // MSB First
            reading |= (1 << (7 - i));
        }
        
        // Pulse Clock
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
    // Start Timer (10ms interval)
    // We use a static variable for the timer struct so it persists
    static struct repeating_timer timer;
    add_repeating_timer_ms(scan_time, reading_timer_callback, NULL, &timer);
}

uint8_t buttons_get_raw_state(void) {
    return current_button_states;
}

uint8_t buttons_get_just_pressed(void) {
    // Snapshot volatile state
    uint8_t current = current_button_states;
    
    // Calculate rising edges (0 -> 1)
    uint8_t just_pressed = (current ^ last_button_states) & current;
    
    // Update history
    last_button_states = current;
    
    return just_pressed;
}


//CHAT MADE THESE FUNCTIONS BELOW!!!
//maps buttons to characters for use in jukebox, allows for multibutton presses
char buttons_map_to_char_jukebox(void) {
    uint8_t edge = ~buttons_get_raw_state();       // Is a button HELD
    // uint8_t edge = buttons_get_just_pressed();   // Was a button CLICKED

    // Identify if the modifier (SELECT) is currently being held
    bool select_held = (edge & BTN_SELECT);
    // Process the "Just Pressed" buttons based on the modifier
    if (edge == 0) return 0; // No new press detected

    if (!select_held) {
        // --- Standard actions (just button) ---
        if (edge & BTN_A)     return 'p'; // B = pause
        if (edge & BTN_B)     return 's'; // A = stop
        if (edge & BTN_U)     return 'u'; // Up = Volume Up
        if (edge & BTN_D)     return 'd'; // Down = Volume Down
        if (edge & BTN_R)     return 'n'; // Right = next song
        if (edge & BTN_L)     return 'o'; // Left = prev song
        if (edge & BTN_START) return 'v'; //change visualizer
    } else {
        // --- modifier actions (buton + select) ---
        if (edge & BTN_R) return 'f'; // Select + Right = Fast Forward
        if (edge & BTN_L) return 'r'; // Select + Left = Rewind
        if (edge & BTN_U) return '+';
        if (edge & BTN_D) return '-';
        if (edge & BTN_A) return 'e'; 
        if (edge & BTN_START) return 'm';
        if (edge & BTN_B) return 'l'; // turn off VU meter
    }
    return 0; // No match found
}

/**
 * 2. FOR MAIN MENU: Maps buttons to navigation
 * Returns: 'U'(Up), 'D'(Down), 'L'(-5), 'R'(+5), 'E'(Enter/Start)
 */
char buttons_map_menu_navigation(void) {
    uint8_t edge = ~buttons_get_raw_state();       // Is a button HELD
    if (edge == 0) return 0;
    if (edge & BTN_U)     return 'u';
    if (edge & BTN_D)     return 'd';
    if (edge & BTN_L)     return 'l';
    if (edge & BTN_R)     return 'r';
    if (edge & BTN_A)     return 'p';
    if (edge & BTN_B)     return 'm';
    return 0;
}

void buttons_sync_state(void) {
    // 1. Force a manual read of the shift register right now
    reading_timer_callback(NULL); 
    
    // 2. Fast-forward the history so no "edges" are detected
    // from whatever buttons are currently being held down.
    last_button_states = current_button_states;
}

#define TIME_TO_REPEAT 1000
#define REPEAT_TIME 100
char prev_char;
absolute_time_t timeout;
    //REPEAT_TIME is in milliseconds (10^-3)
char get_button_repeat(char input_char){

    absolute_time_t t = get_absolute_time();

    if (prev_char != input_char){
        prev_char = input_char;
        timeout = make_timeout_time_ms(TIME_TO_REPEAT);
        return input_char;
    }
    else{
        //if nothing pressed stop
        if (input_char == 0) {
            return 0;
        }
        //special keys to not repeat
        if (input_char == 'p' || input_char == 's' || input_char == 'v') {
            return 0; 
        }
        if (absolute_time_min(t, timeout) == timeout){
            timeout = make_timeout_time_ms(REPEAT_TIME);
            return input_char;
        }
        else{
            return 0;
        }
    }
}
