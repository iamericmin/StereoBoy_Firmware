#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include <stdint.h>
#include "pico/time.h"
#include <math.h>
#include "lib/sb_util/global_vars.h"
#include "dac.h"

#define DEBOUNCE_US 1000000  // 1000 ms


#define TLV_RESET_PIN 1
#define DAC_I2C_ADDR 0x18


#define DAC_VOL_MIN   0x00
#define DAC_VOL_MAX   0x60
#define DAC_VOL_STEP  3      // 1.5 dB step

#define DAC_INT_GPIO 3   // Pico pin connected to TLV320 GPIO1/INT1

#define SCALE_FACTOR_16 32768.0
#define MAX_16BIT 32767 //2^15 - 1 (16 bits, 2's comp)
#define MIN_16BIT -32768



static uint8_t dac_volume = 0x20; // default DAC volume


uint8_t dac_read(uint8_t page, uint8_t reg) {
    uint8_t val;


    // 1. Select page
    uint8_t page_buf[2] = {0x00, page};
    i2c_write_blocking(i2c0, DAC_I2C_ADDR, page_buf, 2, false);


    // 2. Write register address
    i2c_write_blocking(i2c0, DAC_I2C_ADDR, &reg, 1, true);  // no stop, repeated start


    // 3. Read back value
    i2c_read_blocking(i2c0, DAC_I2C_ADDR, &val, 1, false);


    return val;
}


// Helper to write a register on the DAC
void dac_write(uint8_t page, uint8_t reg, uint8_t val) {
    // Set page first
    i2c_write_blocking(i2c0, DAC_I2C_ADDR, (uint8_t[]){0x00, page}, 2, false);
    // Write register
    i2c_write_blocking(i2c0, DAC_I2C_ADDR, (uint8_t[]){reg, val}, 2, false);
}


void dac_increase_volume(uint8_t step) {
    if (dac_volume > DAC_VOL_MIN + step) {
        dac_volume -= step;
    } else {
        dac_volume = DAC_VOL_MIN;
    }


    dac_write(1, 0x24, dac_volume);
    dac_write(1, 0x25, dac_volume);
    dac_write(1, 0x26, dac_volume);
}


void dac_decrease_volume(uint8_t step) {
    if (dac_volume < DAC_VOL_MAX - step) {
        dac_volume += step;
    } else {
        dac_volume = DAC_VOL_MAX;
    }


    dac_write(1, 0x24, dac_volume);
    dac_write(1, 0x25, dac_volume);
    dac_write(1, 0x26, dac_volume);
}


void dac_set_volume(uint8_t vol) {
    if (vol < DAC_VOL_MIN) vol = DAC_VOL_MIN;
    if (vol > DAC_VOL_MAX) vol = DAC_VOL_MAX;


    dac_write(1, 0x24, vol);
    dac_write(1, 0x25, vol);
    dac_write(1, 0x26, vol);
}


// gets volume of left headphone
// hopefully left and right are equal
uint8_t dac_get_volume() {
    uint8_t vol = dac_read(1, 0x24);
    // dac_read(1, 0x25);


    return vol & 0b01111111;
}


// Convert float to 2-byte Hex array (MSB, LSB)
void floatToHex16(double value, uint8_t *hexBuffer) {
    // 1. Scale the float
    int32_t fixedPoint = (int32_t)(value * SCALE_FACTOR_16);
    
    // 2. Clamp to 16-bit range
    if (fixedPoint > MAX_16BIT) fixedPoint = MAX_16BIT;
    if (fixedPoint < MIN_16BIT) fixedPoint = MIN_16BIT;

    // 3. Force to unsigned to prevent C compiler shifting anomalies
    uint16_t unsigned_val = (uint16_t)fixedPoint;

    // 4. Store as Big Endian
    hexBuffer[0] = (unsigned_val >> 8) & 0xFF; // Upper 8 bits
    hexBuffer[1] = unsigned_val & 0xFF;        // Lower 8 bits
}


// Frequencies SPOTIFY:
// static const float eq_frequencies[NUM_EQ_BANDS] = {60, 150, 400, 1000, 2400, 15000};
// person frequency band
static const float eq_frequencies[NUM_EQ_BANDS] = {100, 250, 1000, 3000, 6000, 12000};
// Current Gain State
static float eq_gains[NUM_EQ_BANDS] = {0};
// Current Buffer

// --- INTERNAL HARDWARE UPDATER ---
// This function calculates ALL 6 bands, writes them to the INACTIVE memory, and swaps.
// --- INTERNAL HARDWARE UPDATER ---
static void dac_apply_eq(float sampleRate) {
    
    // 1. Ask the chip which buffer is currently SAFE to write to
    // Read Page 8, Register 1 (Table 6-120)
    uint8_t status = dac_read(8, 1);
    uint8_t targetPage;

    // Check Bit 1 (DAC Adaptive Filter Buffer Control Flag)
    if ((status & 0x02) == 0) { 
        // Flag is 0: DSP is reading Buffer A (Page 8). We must write to Buffer B (Page 12).
        targetPage = 12;
    } else {
        // Flag is 1: DSP is reading Buffer B (Page 12). We must write to Buffer A (Page 8).
        targetPage = 8;
    }

    // 2. Calculate and write all 6 bands to the target page
    for (int band = 0; band < NUM_EQ_BANDS; band++) {

        float freqHz = eq_frequencies[band];
        float gaindB = eq_gains[band];
        float Q = 1.0f;

       // --- MATH SECTION ---
        double A = pow(10, gaindB / 40.0);
        double omega = 2.0 * M_PI * freqHz / sampleRate;
        double sn = sin(omega);
        double cs = cos(omega);
        double alpha = sn / (2.0 * Q);

        // Standard RBJ Biquad Calculations
        double b0 = 1 + (alpha * A);
        double b1 = -2 * cs;
        double b2 = 1 - (alpha * A);
        double a0 = 1 + (alpha / A);
        double a1 = -2 * cs;
        double a2 = 1 - (alpha / A);

        // --- TI HARDWARE MAPPING ---
        // Numerator (N0, N1, N2)
        // Notice ONLY b1 is divided by 2
        double N0_val = b0 / a0;
        double N1_val = b1 / (a0 * 2.0); 
        double N2_val = b2 / a0;

        // Denominator (D1, D2)
        // TI subtracts the feedback, so we negate a1 and a2. 
        // Notice ONLY a1 is divided by 2.
        double D1_val = -a1 / (a0 * 2.0);
        double D2_val = -a2 / a0;

        // --- CONVERSION SECTION ---
        uint8_t n0[2], n1[2], n2[2], d1[2], d2[2];
        floatToHex16(N0_val, n0);
        floatToHex16(N1_val, n1);
        floatToHex16(N2_val, n2);
        floatToHex16(D1_val, d1);
        floatToHex16(D2_val, d2);

        // --- WRITE SECTION ---
        // Use the correct register offsets as seen in Table 6-121
        uint8_t baseLeft = 2 + (band * 10);
        uint8_t baseRight = 66 + (band * 10);

        // Write Left Channel to the inactive page
        dac_write(targetPage, baseLeft + 0, n0[0]); dac_write(targetPage, baseLeft + 1, n0[1]);
        dac_write(targetPage, baseLeft + 2, n1[0]); dac_write(targetPage, baseLeft + 3, n1[1]);
        dac_write(targetPage, baseLeft + 4, n2[0]); dac_write(targetPage, baseLeft + 5, n2[1]);
        dac_write(targetPage, baseLeft + 6, d1[0]); dac_write(targetPage, baseLeft + 7, d1[1]);
        dac_write(targetPage, baseLeft + 8, d2[0]); dac_write(targetPage, baseLeft + 9, d2[1]);

        // Write Right Channel to the SAME inactive page
        dac_write(targetPage, baseRight + 0, n0[0]); dac_write(targetPage, baseRight + 1, n0[1]);
        dac_write(targetPage, baseRight + 2, n1[0]); dac_write(targetPage, baseRight + 3, n1[1]);
        dac_write(targetPage, baseRight + 4, n2[0]); dac_write(targetPage, baseRight + 5, n2[1]);
        dac_write(targetPage, baseRight + 6, d1[0]); dac_write(targetPage, baseRight + 7, d1[1]);
        dac_write(targetPage, baseRight + 8, d2[0]); dac_write(targetPage, baseRight + 9, d2[1]);
    }

    // 3. Trigger the Buffer Swap
    // Write 0x05 (0b00000101) to keep Adaptive Filtering enabled (Bit 2) 
    // and trigger the switch at the next frame boundary (Bit 0).
    dac_write(8, 1, 0x05); 
}

// --- PUBLIC API ---
void dac_eq_init(float sampleRate) {
    // Tell DAC to use Buffer A to start
    
    // Reset all gains to flat
    for(int i=0; i<NUM_EQ_BANDS; i++) {
        eq_gains[i] = 0.0f;
    }
    
    // Push the flat EQ to memory
    dac_apply_eq(sampleRate);
}

void dac_eq_adjust(int band, float step_db, float sampleRate) {
    if (band < 0 || band >= NUM_EQ_BANDS) return;

    // Update state
    eq_gains[band] += step_db;
    if (eq_gains[band] > MAX_GAIN_DB) eq_gains[band] = MAX_GAIN_DB;
    if (eq_gains[band] < MIN_GAIN_DB) eq_gains[band] = MIN_GAIN_DB;

    // Recalculate and push to hardware
    dac_apply_eq(sampleRate);
}


//Geters for variables
float dac_eq_get_gain(int band) {
    if (band < 0 || band >= NUM_EQ_BANDS) return 0.0f;
    return eq_gains[band];
}

int dac_eq_get_freq(int band) {
    if (band < 0 || band >= NUM_EQ_BANDS) return 0;
    return (int)eq_frequencies[band];
}

void dac_init() {
    // 1. Hardware Reset
    gpio_init(TLV_RESET_PIN);
    gpio_set_dir(TLV_RESET_PIN, GPIO_OUT);
    gpio_put(TLV_RESET_PIN, 0);
    sleep_ms(100);
    gpio_put(TLV_RESET_PIN, 1);
    sleep_ms(10);

    // 2. Software Reset (Page 0, Reg 1)
    dac_write(0, 0x01, 0x01);
    sleep_ms(10);

    // Enable PRB_P2 (best for EQ)
    dac_write(0, 60, 0b00000001);
    printf("asdf\r\n");
    //Enable adaptive filtering (so eq can change in real time)
    dac_write(8, 1, 0x04);

    // 3. Interface Control (I2S, 16-bit)
    // Reg 0x1B: 0x00 = I2S, 16bit
    dac_write(0, 0x1B, 0x00);


    // 4. Clock Setup (PLL from BCLK)
    // Reg 0x04: CLKOUT Mux. PLL_CLKIN = BCLK (0x07 is typical for BCLK)
    // Your code used setCodecClockInput(PLL) and setPLLClockInput(BCLK)
    dac_write(0, 0x04, 0x07);


    // 5. PLL Values (P=1, R=2, J=32, D=0)
    // Reg 0x05: P and R. Bit 7=PowerUp. P=1 (bits 6:4), R=2 (bits 3:0)
    // Note: Library uses P=1, but 0 in register often means 1. Check datasheet.
    dac_write(0, 0x05, 0x92); // 1001 0010 -> PLL Power On, P=1, R=2
    dac_write(0, 0x06, 32);   // J=32
    dac_write(0, 0x07, 0);    // D MSB
    dac_write(0, 0x08, 0);    // D LSB


    // 6. DAC Dividers (NDAC=8, MDAC=2)
    dac_write(0, 0x0B, 0x88); // NDAC Power on, Val=8
    dac_write(0, 0x0C, 0x82); // MDAC Power on, Val=2
    // DOSR = 128 (0x0080) - Standard for 44.1/48k
    dac_write(0, 0x0D, 0x00);
    dac_write(0, 0x0E, 0x80);


    // 7. DAC Data Path
    // Reg 0x3F: Left/Right DAC Power Up, Normal Path
    // DAC left = left channel audio
    // DAC right = right channel audio

    if (dac_read(0, 0x2E) & 0x10) { // read whether headphone in or out
        dac_write(0, 0x3F, 0b11010110); // if headphones inserted, stereo
    } else {
        dac_write(0, 0x3F, 0b11111110); // if speaker, mono
    }


    // 8. Routing (Page 1)
    // Reg 0x23: DAC to Mixer
    dac_write(1, 0x23, 0x44); 


    // 9. DAC Volume (Page 0)
    dac_write(0, 0x40, 0x00); // Unmute
    //to prevent clipping from eq
    // dac_write(0, 0x41, 0xE8); // Left Vol -12dB (E8) -20dB -> (D8)
    // dac_write(0, 0x42, 0xE8); // Right Vol -12dB
    dac_write(0, 0x41, 00);   // Left Vol (0dB is usually 0, 18 is +9dB depending on mapping)
    dac_write(0, 0x42, 00);   // Right Vol

    // 10. Headphone & Speaker Setup (Page 1)
    // Reg 0x24/0x25: HPL/R Gain (0x06 = 6dB approx)
    // ADJUST THESE VALUES FOR VOLUME CONTROL IN MAIN FIRMWARE!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    dac_write(1, 0x24, 0x05);
    dac_write(1, 0x25, 0x05);
    dac_write(1, 0x26, 0x05);
    // Reg 0x1F: HP Drivers power up
    dac_write(1, 0x1F, 0xC0);
    // Reg 0x28/0x29: HPL/R Driver unmute
    dac_write(1, 0x28, 0x06);
    dac_write(1, 0x29, 0x06);


    // Speaker
    dac_write(1, 0x20, 0b10000110); // Speaker Power Up, 6dB gain
    dac_write(1, 0x2A, 0b00010100); // Speaker Unmute / Gain


    // 11. Headset Detect
    dac_write(1, 0x2E, 0x0b);  // MICBIAS enable


    // Route headset detect interrupt to GPIO1
    dac_write(0, 0x43, 0b10010101);  // Enable headset detect + interrupt
    dac_write(0, 0x30, 0b10000001); // route INT1 to headset change interrupt
    dac_write(0, 0x33, 0b00010100); // GPIO1 as INT1 output


    dac_set_volume((uint32_t)potVal * 0x60 / 4096);
    
    // Give the amp a tiny moment to attempt power-up before checking faults
    sleep_ms(10); 
    
    printf("--- Checking DAC Faults ---\r\n");
    uint8_t tlv_addr = 0x18; 
    uint8_t fault_reg = 0;
    
    // 1. Set the page register (Register 0) to Page 0
    uint8_t page_data[2] = {0x00, 0x00};
    i2c_write_blocking(i2c_default, tlv_addr, page_data, 2, false);
    
    // 2. Write the register address we want to read (Reg 44 / 0x2C)
    uint8_t reg_addr = 44;
    i2c_write_blocking(i2c_default, tlv_addr, &reg_addr, 1, true); // true = keep master control
    
    // 3. Read the byte back
    i2c_read_blocking(i2c_default, tlv_addr, &fault_reg, 1, false);
    
    printf("Sticky Flag Reg (0x2C): 0x%02X\r\n", fault_reg);
    
    if (fault_reg & 0b10000000) { 
        printf("FAULT [Bit 7]: Short Circuit or Overcurrent detected!\r\n");
    }
    if (fault_reg & 0b00100000) { 
        printf("FAULT [Bit 5]: Speaker Amplifier Power-up Incomplete (Brownout)!\r\n");
    }
    if (fault_reg == 0x00) {
        printf("STATUS: No faults detected. Chip thinks it is happy.\r\n");
    }
    printf("---------------------------\r\n");
    // --- END PASTE BLOCK ---
}