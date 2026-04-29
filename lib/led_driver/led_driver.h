#ifndef PCA9685_H
#define PCA9685_H

#include "lib/sb_util/global_vars.h"


#define PCA9685_I2C_ADDR 0x40
#define PCA9685_OSC_FREQ 25000000
#define NUM_LEDS_PER_CH 8
#define ADC_CENTER 1551
#define MAX_AMPLITUDE 600.0f

// I2C1 for LED Driver
#define PIN_I2C1_SDA 42
#define PIN_I2C1_SCL 43

bool pca9685_init(pca9685_t *dev, i2c_inst_t *i2c, uint8_t addr);

void pca9685_reset(pca9685_t *dev);
void pca9685_sleep(pca9685_t *dev);
void pca9685_wakeup(pca9685_t *dev);

bool pca_check_presence(pca9685_t *dev);

bool pca9685_checkSleep(pca9685_t *dev);
void pca9685_toggleSleep(pca9685_t *dev);

void pca9685_set_pwm_freq(pca9685_t *dev, float freq);
void pca9685_set_pwm(pca9685_t *dev, uint8_t channel, uint16_t on, uint16_t off);
void pca9685_set_pin(pca9685_t *dev, uint8_t channel, uint16_t value);
void pca9685_write_microseconds(pca9685_t *dev, uint8_t channel, uint16_t us);
void pca9685_update_vu(pca9685_t *dev, uint16_t adc_left, uint16_t adc_right);

int pca9685_get_brightness();
void pca9685_set_brightness(int new_brightness);
void pca9685_increase_brightness(float x);
void pca9685_decrease_brightness(float x);
void pca9685_update_brightness(float x);

#endif
