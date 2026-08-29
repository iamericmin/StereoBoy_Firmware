#ifndef TLV320_PICO_H
#define TLV320_PICO_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define TLV320_ADDR 0x18

// Essential Registers
#define DAC_REG_PAGE_SELECT     0x00
#define DAC_REG_CLOCK_MUX1      0x04
#define DAC_REG_PLL_J           0x06
#define DAC_REG_PLL_D_MSB       0x07
#define DAC_REG_PLL_D_LSB       0x08
#define DAC_REG_PLL_P_R         0x05
#define DAC_REG_NDAC            0x0B
#define DAC_REG_MDAC            0x0C
#define DAC_REG_DOSR_MSB        0x0D
#define DAC_REG_DOSR_LSB        0x0E
#define DAC_REG_RESET           0x01
#define DAC_REG_CODEC_IF_CTRL1  0x1B // Format and word length
#define DAC_REG_PRB             0x3C
#define DAC_REG_DAC_DATAPATH    0x3F // Power up DACs
#define DAC_REG_DAC_VOL_CTRL    0x40 // Mute/Unmute
#define DAC_REG_DAC_LVOL        0x41
#define DAC_REG_DAC_RVOL        0x42

// Page 1 Registers
#define DAC_REG_HP_DRIVERS      0x1F // Headphone power
#define DAC_REG_SPK_AMP         0x20 // Speaker power
#define DAC_REG_HP_DEPOP        0x21 // Headphone depop settings
#define DAC_REG_HP_PGA_L        0x24 // HPL Volume
#define DAC_REG_HP_PGA_R        0x25 // HPR Volume
#define DAC_REG_SPK_PGA         0x26 // Speaker Volume
#define DAC_REG_DAC_OUTPUT_MIX  0x23 // DAC L & R output mixer routing
#define DAC_REG_HPL_DRIVER      0x28 // HPL driver
#define DAC_REG_HPR_DRIVER      0x29 // HPR driver
#define DAC_REG_SPK_DRIVER      0x2A // Class-D driver

//Constants for chaning eq
#define NUM_EQ_BANDS 6
#define MAX_GAIN_DB 6.0f
#define MIN_GAIN_DB -6.0f


uint8_t dac_read(uint8_t page, uint8_t reg);
void dac_write(uint8_t page, uint8_t reg, uint8_t val);
void dac_init();
bool dac_begin(i2c_inst_t *i2c);
void dac_set_volume(uint8_t vol);
void dac_increase_volume(uint8_t step);
void dac_decrease_volume(uint8_t step);
uint8_t dac_get_volume();
void dac_interrupt_init(void);

//EQ functions:
void dac_eq_init(float sampleRate);            
void dac_eq_adjust(int band, float step_db, float sampleRate); 
float dac_eq_get_gain(int band);   
int dac_eq_get_freq(int band);    
static void dac_apply_eq(float sampleRate);


void dac_int_callback(uint gpio, uint32_t events);
void dac_interrupt_init(void);


#endif