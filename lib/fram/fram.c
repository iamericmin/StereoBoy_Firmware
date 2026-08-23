#ifndef FM24C64B_H
#define FM24C64B_H

#include "hardware/i2c.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define FM24C64B_I2C_ADDR 0x50

/**
 * Write bytes to FM24C64B F-RAM.
 * @param i2c i2c0 or i2c1 hardware instance
 * @param mem_addr 16-bit memory address (0x0000 to 0x1FFF)
 * @param src Pointer to source data
 * @param len Number of bytes to write
 * @return Number of payload bytes written, or PICO_ERROR_GENERIC on failure
 */
int fram_write(i2c_inst_t *i2c, uint16_t mem_addr, const uint8_t *src, size_t len) {
    // Allocate buffer for 2-byte address + payload
    size_t total_len = 2 + len;
    
    // Stack buffer for small writes (under 64 bytes) to avoid heap allocation overhead
    uint8_t stack_buf[66];
    uint8_t *tx_buf = (total_len <= sizeof(stack_buf)) ? stack_buf : (uint8_t *)malloc(total_len);

    if (!tx_buf) return PICO_ERROR_GENERIC;

    tx_buf[0] = (uint8_t)(mem_addr >> 8);   // Address MSB
    tx_buf[1] = (uint8_t)(mem_addr & 0xFF); // Address LSB
    memcpy(&tx_buf[2], src, len);

    // Single continuous write sequence: [ADDR_MSB][ADDR_LSB][DATA_0][DATA_1]...
    int ret = i2c_write_blocking(i2c, FM24C64B_I2C_ADDR, tx_buf, total_len, false);

    if (tx_buf != stack_buf) {
        free(tx_buf);
    }

    return (ret == (int)total_len) ? (int)len : PICO_ERROR_GENERIC;
}

/**
 * Read bytes from FM24C64B F-RAM.
 * @param i2c i2c0 or i2c1 hardware instance
 * @param mem_addr 16-bit memory address (0x0000 to 0x1FFF)
 * @param dst Pointer to destination buffer
 * @param len Number of bytes to read
 * @return Number of bytes read, or PICO_ERROR_GENERIC on failure
 */
int fram_read(i2c_inst_t *i2c, uint16_t mem_addr, uint8_t *dst, size_t len) {
    uint8_t buf[2];
    buf[0] = (uint8_t)(mem_addr >> 8);   // Address MSB
    buf[1] = (uint8_t)(mem_addr & 0xFF); // Address LSB

    // Step 1: Set internal memory pointer (nostop = true for repeated START condition)
    if (i2c_write_blocking(i2c, FM24C64B_I2C_ADDR, buf, 2, true) != 2) {
        return PICO_ERROR_GENERIC;
    }

    // Step 2: Read payload into destination buffer
    return i2c_read_blocking(i2c, FM24C64B_I2C_ADDR, dst, len, false);
}

#endif // FM24C64B_H