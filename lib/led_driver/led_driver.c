#include "led_driver.h"
#include "pico/stdlib.h"
#include <math.h>
#include <stdlib.h>

/* Registers */
#define MODE1         0x00
#define MODE2         0x01
#define PRESCALE      0xFE
#define LED0_ON_L     0x06
#define ALL_LED_ON_L  0xFA
#define ALL_LED_ON_H  0xFB
#define ALL_LED_OFF_L 0xFC
#define ALL_LED_OFF_H 0xFD

/* MODE1 Bits */
#define MODE1_RESTART 0x80
#define MODE1_AI      0x20
#define MODE1_SLEEP   0x10

/* MODE2 Bits */
#define MODE2_INVRT   0x10  // Invert logic: 1 = High duty cycle is Sink (GND)
#define MODE2_OUTDRV  0x04  // 0 = Open-Drain, 1 = Totem-Pole

#define MAX_BRIGHTNESS 4095
#define DESIRED_BRIGHTNESS 32

static void write8(pca9685_t *dev, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_write_blocking(dev->i2c, dev->addr, buf, 2, false);
}

static uint8_t read8(pca9685_t *dev, uint8_t reg) {
    i2c_write_blocking(dev->i2c, dev->addr, &reg, 1, true);
    uint8_t val;
    i2c_read_blocking(dev->i2c, dev->addr, &val, 1, false);
    return val;
}

bool pca9685_init(pca9685_t *dev, i2c_inst_t *i2c, uint8_t addr) {
    dev->i2c = i2c;
    dev->addr = addr;
    dev->osc_freq = PCA9685_OSC_FREQ;

    // 1. Put to sleep so we can safely configure registers & prescaler
    pca9685_sleep(dev);

    // 2. Set MODE2: Inverted logic (1) for common cathode driving
    write8(dev, MODE2, MODE2_INVRT);

    // 3. Set Auto-Increment (MODE1_AI) while staying in sleep
    write8(dev, MODE1, MODE1_SLEEP | MODE1_AI);

    // 4. Force ALL PWM outputs to OFF before waking up the oscillator
    pca9685_all_off(dev);

    // 5. Set PWM frequency (handles sleep/wake transitions internally)
    pca9685_set_pwm_freq(dev, 1000);

    // 6. Wake up: clear SLEEP bit while keeping Auto-Increment
    write8(dev, MODE1, MODE1_AI);
    sleep_ms(5); // Allow internal oscillator time to stabilize

    // 7. Clear restart bit to activate PWM output channels
    write8(dev, MODE1, MODE1_AI | MODE1_RESTART);

    return true;
}

void pca9685_set_pwm(pca9685_t *dev, uint8_t channel, uint16_t on, uint16_t off) {
    uint8_t reg = LED0_ON_L + (4 * channel);
    uint8_t buf[5] = {
        reg,
        (uint8_t)(on & 0xFF),
        (uint8_t)((on >> 8) & 0x1F), // Masking to 5 bits (includes Full ON bit)
        (uint8_t)(off & 0xFF),
        (uint8_t)((off >> 8) & 0x1F) // Masking to 5 bits (includes Full OFF bit)
    };
    i2c_write_blocking(dev->i2c, dev->addr, buf, 5, false);
}

/**
 * Sets brightness from 0 to 4095. 
 * Hardware INVRT handles the cathode logic.
 */
void pca9685_set_pin(pca9685_t *dev, uint8_t channel, uint16_t value) {
    if (value > 4095) value = 4095;

    if (value == 4095) {
        // Special Case: Full ON (Bit 4 of ON_H)
        pca9685_set_pwm(dev, channel, 0x1000, 0);
    } else if (value == 0) {
        // Special Case: Full OFF (Bit 4 of OFF_H)
        pca9685_set_pwm(dev, channel, 0, 0x1000);
    } else {
        // Standard PWM: We pulse from time 0 to 'value'
        pca9685_set_pwm(dev, channel, 0, value);
    }
}

bool pca9685_checkSleep(pca9685_t *dev) {
    return (read8(dev, MODE1) & MODE1_SLEEP) != 0;
}

void pca9685_sleep(pca9685_t *dev) {
    uint8_t mode1 = read8(dev, MODE1);
    write8(dev, MODE1, mode1 | (1 << 4));
}

void pca9685_wakeup(pca9685_t *dev) {
    // 1. Ensure all outputs are cleared before restoring PWM clock
    pca9685_all_off(dev);
    sleep_ms(5); // Oscillator spin-up time

    // 2. Clear SLEEP bit (keep Auto-Increment intact)
    uint8_t mode1 = read8(dev, MODE1);
    write8(dev, MODE1, (mode1 & ~MODE1_SLEEP) | MODE1_AI);

    // 3. Trigger RESTART to re-enable PWM control
    write8(dev, MODE1, (mode1 & ~MODE1_SLEEP) | MODE1_AI | MODE1_RESTART);
}

void pca9685_toggleSleep(pca9685_t *dev) {
    if (pca9685_checkSleep(dev)) {
        pca9685_wakeup(dev);
    } else {
        pca9685_sleep(dev);
    }
}

// ... (pca9685_set_pwm_freq remains the same) ...
void pca9685_set_pwm_freq(pca9685_t *dev, float freq) {
    if (freq < 1) freq = 1;
    if (freq > 3500) freq = 3500;

    float prescale_val = ((dev->osc_freq / (freq * 4096.0f)) + 0.5f) - 1.0f;
    uint8_t prescale = (uint8_t)fminf(fmaxf(prescale_val, 3), 255);

    uint8_t oldmode = read8(dev, MODE1);
    write8(dev, MODE1, (oldmode & ~MODE1_RESTART) | MODE1_SLEEP);
    write8(dev, PRESCALE, prescale);
    write8(dev, MODE1, oldmode);
    sleep_ms(5);
    write8(dev, MODE1, oldmode | MODE1_RESTART | MODE1_AI);
}

// State variables for peak smoothing 
static float peak_l = 0.0f;
static float peak_r = 0.0f;
static const float PEAK_DECAY = 0.075f; // How fast the LEDs fall back down

void pca9685_update_vu(pca9685_t *dev, uint16_t adc_left, uint16_t adc_right) {
    float amp_l = (float)abs((int)adc_left - ADC_CENTER);
    float amp_r = (float)abs((int)adc_right - ADC_CENTER);
    
    float level_l = fminf(amp_l / MAX_AMPLITUDE, 1.0f);
    float level_r = fminf(amp_r / MAX_AMPLITUDE, 1.0f);

    if (level_l > peak_l) peak_l = level_l; else peak_l -= PEAK_DECAY;
    if (peak_l < 0.0f) peak_l = 0.0f;

    if (level_r > peak_r) peak_r = level_r; else peak_r -= PEAK_DECAY;
    if (peak_r < 0.0f) peak_r = 0.0f;

    // Loop through 8 LED segments
    for (int i = 0; i < 8; i++) {
        float threshold = (float)(i + 1) / 8.0f;
        float prev_threshold = (float)i / 8.0f;

        // Calculate the physical pin indices
        // Left:  i=0 (Bottom) -> Pin 7, i=7 (Top) -> Pin 0
        // Right: i=0 (Bottom) -> Pin 15, i=7 (Top) -> Pin 8
        uint8_t pin_l = 7 - i;
        uint8_t pin_r = 15 - i;

        // Left Bank Logic
        if (peak_l >= threshold) {
            pca9685_set_pin(dev, pin_l, DESIRED_BRIGHTNESS);
        } else if (peak_l > prev_threshold) {
            float fraction = (peak_l - prev_threshold) * 8.0f;
            pca9685_set_pin(dev, pin_l, (uint16_t)(fraction * DESIRED_BRIGHTNESS));
        } else {
            pca9685_set_pin(dev, pin_l, 0);
        }

        // Right Bank Logic
        if (peak_r >= threshold) {
            pca9685_set_pin(dev, pin_r, DESIRED_BRIGHTNESS);
        } else if (peak_r > prev_threshold) {
            float fraction = (peak_r - prev_threshold) * 8.0f;
            pca9685_set_pin(dev, pin_r, (uint16_t)(fraction * DESIRED_BRIGHTNESS));
        } else {
            pca9685_set_pin(dev, pin_r, 0);
        }
    }
}

void pca9685_all_off(pca9685_t *dev) {
    peak_l = 0.0f;
    peak_r = 0.0f;

    // Use the ALL_LED registers to force everything off in one I2C transaction
    uint8_t buf[5] = {
        ALL_LED_ON_L,
        0x00, 
        0x00, 
        0x00, 
        0x10  // Bit 4 of OFF_H = Full OFF
    };
    i2c_write_blocking(dev->i2c, dev->addr, buf, 5, false);
}