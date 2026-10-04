#include "lib/sb_util/global_vars.h"
#include "buttons.h"

#define HOLD_CYCLES 10

const uint PIN_LATCH = 11;
const uint PIN_CLOCK = 23;
const uint PIN_DATA  = 22;

uint8_t buttons_raw = 0;
uint8_t last_button_states = 0; // Used for edge detection
uint8_t buttons_pressed;
uint8_t buttons_released;

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

    buttons_raw = reading;
    return true;
}

void buttons_init(int32_t scan_time) {
    // GPIO Init
    gpio_init(PIN_LATCH); gpio_set_dir(PIN_LATCH, GPIO_OUT); gpio_put(PIN_LATCH, 1);
    gpio_init(PIN_CLOCK); gpio_set_dir(PIN_CLOCK, GPIO_OUT); gpio_put(PIN_CLOCK, 0);
    gpio_init(PIN_DATA);  gpio_set_dir(PIN_DATA, GPIO_IN);
    
    // Force a manual read of the shift register right now
    reading_timer_callback(NULL); 
    
    // Fast-forward the history so no "edges" are detected
    // from whatever buttons are currently being held down.
    last_button_states = buttons_raw;

    // We use a static variable for the timer struct so it persists
    static struct repeating_timer timer;
    add_repeating_timer_ms(scan_time, reading_timer_callback, NULL, &timer);
}

uint8_t buttons_get_raw_state(void) {
    return buttons_raw;
}

void buttons_get_edges(void) {
    uint8_t current = buttons_raw;
    buttons_pressed  = ~current &  last_button_states; // Active-low press (1 -> 0)
    buttons_released =  current & ~last_button_states; // Active-low release (0 -> 1)
    last_button_states = current;
}

uint8_t buttons_get_action() {
    static uint8_t hold_counter;
    static uint8_t buttons_current;
    static uint8_t buttons_prev;
    buttons_get_edges();
    printf("Pressed: %08b\n", buttons_pressed);
    printf("Released: %08b\n", buttons_released);
    if (buttons_pressed) { // if falling edge detected (buttons are active low)
        buttons_current = buttons_raw; // capture raw button states
        hold_counter = 0; // reset hold counter to zero
    } else if (buttons_raw != 0xFF) { // if holding
        // These are buttons that trigger auto-fire after a ~500ms delay
        if (buttons_raw == BTN_L || buttons_raw == BTN_R || buttons_raw == BTN_U || buttons_raw == BTN_D) {
            hold_counter++;
            if (hold_counter >= HOLD_CYCLES) {
                buttons_current = buttons_raw;
            } else {
                buttons_current = 0xFF;
            }
        // These are buttons that auto-fire right away with no delay
        } else if ((buttons_raw == (BTN_B & BTN_L)) || (buttons_raw == (BTN_B & BTN_R))) {
            buttons_current = buttons_raw;
        // These are buttons that fire once and never trigger again until it's pressed again
        } else if ((buttons_raw == BTN_A) || (buttons_raw == BTN_B)) {
            hold_counter = 0;
            buttons_current = 0xFF;
        } else {
            buttons_current = 0xFF;
        }
    } else {
        hold_counter = 0;
        buttons_current = 0xFF;
    }
    buttons_prev = buttons_raw;


    return buttons_current;
}
