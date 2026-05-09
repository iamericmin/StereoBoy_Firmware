#pragma once
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <stdbool.h>
#include "lib/sb_util/global_vars.h"

#define PIN_SCK 30
#define PIN_MOSI 28
#define PIN_MISO 31
#define PIN_CS 32

#define MAX_FILENAME_LEN 256

void sci_write(vs1053_t *v, uint8_t addr, uint16_t data);
uint16_t sci_read(vs1053_t *v, uint8_t addr);
void vs1053_init(vs1053_t *v);
void vs1053_soft_reset(vs1053_t *v);
void vs1053_set_volume(vs1053_t *v, uint8_t left, uint8_t right);
void vs1053_play_data(vs1053_t *v, const uint8_t *data, size_t len);
bool vs1053_ready(vs1053_t *v);
void vs1053_stop(vs1053_t *v);
void vs1053_set_play_speed(vs1053_t *player, uint16_t speed);
uint16_t vs1053_get_play_speed(vs1053_t *player);
void vs1053_enable_i2s(vs1053_t *v);
void vs1053_set_samplerate(vs1053_t *player, uint16_t samplerate);
void vs1053_load_patch(vs1053_t *v, const unsigned short* plugin, unsigned short plugin_size);
void vs1053_tape_stop(vs1053_t *v);
void adjustRate(vs1053_t *v, long ppm2);
