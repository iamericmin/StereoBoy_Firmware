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
#define PI 3.14159f

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

typedef struct st7789_t {
    spi_inst_t* spi;
    uint gpio_din;
    uint gpio_clk;
    int gpio_cs;
    uint gpio_dc;
    uint gpio_rst;
    uint gpio_bl;
} st7789_t;

//CODEC
typedef struct {
    spi_inst_t *spi;
    uint cs;
    uint dcs;
    uint dreq;
    uint rst;
} vs1053_t;

extern vs1053_t player;
extern st7789_t display;

//POT

//SB_UTIL
#define MAX_FILENAME_LEN 128 // max filaname character length
#define MAX_TRACKS 128 // max number of tracks to hold in metadata buffer
#define MAX_FOLDERS 32

extern mutex_t text_buff_mtx;
extern semaphore_t text_sem;
extern int visualizer;
extern bool album_art_ready;

#define IMG_WIDTH 160
#define IMG_HEIGHT 160
extern uint16_t img_buffer[IMG_WIDTH * IMG_HEIGHT];

typedef struct __attribute__((packed)) {
    uint32_t audio_start; 
    uint32_t audio_end;   
    uint32_t header;
    uint16_t bitrate;
    uint16_t samplespeed;
    uint8_t mpegID;
    uint8_t channels;
    char filename[256];
    char title[128];
    char artist[128];
    char album[128];
} track_info_t;

extern track_info_t *current_track;
extern track_info_t current_track_holder;

typedef struct __attribute__((packed)) {
    char artist_name[128];
    uint16_t num_albums;
    uint16_t start_album;
} artist_info_t;

typedef struct __attribute__((packed)) {
    char album_name[128];
    uint16_t num_tracks;
    uint16_t start_track;
} album_info_t;

extern track_info_t track_window[10];
extern artist_info_t artist_window[10];
extern album_info_t album_window[10];

// Global runtime database pointers
extern artist_info_t *global_artists;
extern album_info_t  *global_albums;

// read-only cache file that contains every track's metadata
extern FIL tracks_cache_file;

// Global counts for UI loop boundaries
extern uint16_t artist_count;
extern uint16_t album_count;
extern uint16_t track_count;

typedef struct {
    char foldername[64];
    uint8_t num_tracks;
} folder_info_t;

#pragma pack(push, 1)
typedef struct {
    uint64_t hash;
    uint32_t pointer;
} LUT_entry_t;
#pragma pack(pop)

extern LUT_entry_t *artCache_LUT;
extern uint32_t lut_entry_count;

extern int count;
extern uint16_t song_choice;
extern uint16_t album_choice;
extern uint16_t menu_choice;

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

// ST7789 uses 16-bit RGB565 colors
extern uint16_t played_progres_color;
extern uint16_t background_progress_color;

#define played_progres_color 0xFFFF
#define background_progress_color 0x0000
extern int selected_band;
extern volatile uint16_t potVal;



#endif