#ifndef FRAM_H
#define FRAM_H

#define FRAM_ADDR 0x50

int fram_write(i2c_inst_t *i2c, uint16_t mem_addr, uint8_t *src, size_t len);
int fram_read(i2c_inst_t *i2c, uint16_t mem_addr, uint8_t *dst, size_t len);

#endif
