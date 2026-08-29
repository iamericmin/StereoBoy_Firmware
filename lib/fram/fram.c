#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "stdlib.h"
#include "lib/fram/fram.h"
#include "lib/sb_util/sb_util.h"

int fram_write(i2c_inst_t *i2c, uint16_t mem_addr, uint8_t *src, size_t len) {
    uint8_t *buf = (uint8_t *)malloc(len + 2);
    if (buf == NULL) {
        return PICO_ERROR_GENERIC;
    }

    buf[0] = mem_addr >> 8;
    buf[1] = mem_addr & 0xFF;

    memcpy(&buf[2], src, len);

    uint8_t bytes_written = i2c_write_blocking(i2c, FRAM_ADDR, buf, len + 2, false);

    free(buf);

    return bytes_written;
}

int fram_read(i2c_inst_t *i2c, uint16_t mem_addr, uint8_t *dst, size_t len) {
    uint8_t buf[2];

    buf[0] = mem_addr >> 8;
    buf[1] = mem_addr & 0xFF;

    if (i2c_write_blocking(i2c, FRAM_ADDR, buf, 2, true) != 2) {
        return PICO_ERROR_GENERIC;
    }

    uint8_t bytes_read = i2c_read_blocking(i2c, FRAM_ADDR, dst, len, false);

    return bytes_read;
}