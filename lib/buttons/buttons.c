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
    
    // Force a manual read of the shift register right now
    reading_timer_callback(NULL); 
    
    // Fast-forward the history so no "edges" are detected
    // from whatever buttons are currently being held down.
    last_button_states = current_button_states;

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