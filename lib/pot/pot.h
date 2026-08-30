#ifndef POT
#define POT

#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/timer.h"
#include <stdio.h>
// #include "lib/dac/dac.h"

#define POT_ADC_PIN 44     
#define POT_ADC_CHANNEL 4     // ADC channel 0 maps to GPIO 26
#define LWBT_GPIO 10
#define POLLING_RATE_MS 200    // Read the pot every 200
#define MAX_DAC_VOL 0x60       // Adjust this to your DAC's max register value

bool pot_timer_callback(struct repeating_timer *t);

void pot_init(void);

void read_lwbt();

#endif