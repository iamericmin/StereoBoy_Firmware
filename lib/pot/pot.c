#include "pot.h"
#include "lib/sb_util/global_vars.h"

struct repeating_timer pot_timer;

// State variables for filtering
static uint16_t smoothed_adc = 0;

bool pot_timer_callback(struct repeating_timer *t) {

    return true; 
}

void pot_init(void) {
    // Initialize the ADC hardware (might already do this)
    // adc_init();
    
    // Make sure the GPIO is high-impedance, no pullups
    adc_gpio_init(POT_ADC_PIN);
    gpio_init(LWBT_GPIO);
    gpio_init(LED_R);
    gpio_init(LED_G);
    gpio_init(LED_B);

    gpio_set_dir(LED_R, true);
    gpio_set_dir(LED_G, true);
    gpio_set_dir(LED_B, true);
    gpio_set_dir(LWBT_GPIO, false);

    // Do an initial read to seed the moving average filter instantly
    adc_select_input(POT_ADC_CHANNEL);
    smoothed_adc = adc_read();
    printf("startup ADC: %d", smoothed_adc);

    gpio_put(LED_B, 1);
    read_lwbt();

    // Start the repeating hardware timer
    // add_repeating_timer_ms(POLLING_RATE_MS, pot_timer_callback, NULL, &pot_timer);
    printf("pot init");
}

void read_lwbt(){
    int lwbt = gpio_get(LWBT_GPIO);
    printf("lwbt: %d\r\n", lwbt);
    gpio_put(LED_R, 1-lwbt);
    gpio_put(LED_G, 1-lwbt);
    gpio_put(LED_B, 1-lwbt);
    return;
}   