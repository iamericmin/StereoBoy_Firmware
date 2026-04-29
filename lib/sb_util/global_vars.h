#ifndef GLOBAL_VARS
#define GLOBAL_VARS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <stdint.h>
#include <stdbool.h>

#include "ff.h"
#include "sd_card.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/time.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/spi.h"

#include "lib/display/picojpeg.h"
#include "lib/font/font.h"


//LED DRIVER
typedef struct {
    i2c_inst_t *i2c;
    uint8_t addr;
    uint32_t osc_freq;
} pca9685_t;
extern pca9685_t vu_meter;

//ADC

//DAC
extern bool paused;
extern bool warping;

//DISPLAY
#define HISTORY_SIZE 256

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240

extern uint16_t play_icon[400];
extern uint16_t pause_icon[400];
extern uint16_t empty_icon[400];
extern uint16_t ff_icon[400];
extern uint16_t rew_icon[400];
extern uint16_t frame_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];

extern struct st7789_t st7789_cfg;
extern uint16_t st7789_width;
extern uint16_t st7789_height;
extern bool st7789_data_mode;
extern float x_brightness;

typedef struct st7789_t {
    spi_inst_t* spi;
    uint gpio_din;
    uint gpio_clk;
    int gpio_cs;
    uint gpio_dc;
    uint gpio_rst;
    uint gpio_bl;
} st7789_t;

//POT

//SB_UTIL
#define MAX_FILENAME_LEN 256 // max filaname character length
#define MAX_TRACKS 128 // max number of mp3 files in sd card

extern mutex_t text_buff_mtx;
extern semaphore_t text_sem;
extern int visualizer;
extern bool album_art_ready;

#define IMG_WIDTH 160
#define IMG_HEIGHT 160
extern uint16_t img_buffer[IMG_WIDTH * IMG_HEIGHT];

typedef struct {
    uint32_t album_art_size;
    uint32_t album_art_offset;
    uint32_t audio_start; 
    uint32_t audio_end;   
    uint32_t header;
    uint16_t bitrate;
    uint16_t samplespeed;
    uint8_t mpegID;
    uint8_t channels;
    uint8_t album_art_type;
    char mime_type[32];
    char filename[256];
    char title[128];
    char artist[128];
    char album[128];
} track_info_t;

extern track_info_t tracks[MAX_TRACKS];
extern int count;

//CODEC
typedef struct {
    spi_inst_t *spi;
    uint cs;
    uint dcs;
    uint dreq;
    uint rst;
} vs1053_t;

//FFT
typedef float complex cplx;
extern cplx audio_history_l[HISTORY_SIZE];
extern cplx audio_history_r[HISTORY_SIZE];


//Core 1
struct Node {
    struct Node * next;
    char str[30];
};
extern uint16_t* playStatus;
extern uint16_t* ff_rew_status;
extern int progress_bar;
extern bool enableIcons;
extern int song_choice;

// ST7789 uses 16-bit RGB565 colors
extern uint16_t played_progres_color;
extern uint16_t background_progress_color;

#define played_progres_color 0xFFFF
#define background_progress_color 0x0000
extern int selected_band;
extern volatile uint16_t potVal;



#endif