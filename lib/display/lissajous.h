#ifndef LISSAJOUS_H
#define LISSAJOUS_H

#include <stdint.h>
#include <stdlib.h>
#include <complex.h>

// ---------------------------------------------------------
// Shared Types & Macros
// ---------------------------------------------------------

#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH 240
#endif

#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT 240
#endif

#ifndef HISTORY_SIZE
#define HISTORY_SIZE 256
#endif

// ADC Mapping Constants needed for scaling the Lissajous figures
#ifndef ADC_BIAS_CENTER
#define ADC_BIAS_CENTER 1551
#endif

#ifndef ADC_RANGE_PKPK
#define ADC_RANGE_PKPK 1613
#endif

// ---------------------------------------------------------
// External Global Variables
// ---------------------------------------------------------
// These variables live in your main C file but are needed here 
// to read audio samples and draw the output.

// extern cplx audio_history_l[HISTORY_SIZE];
// extern cplx audio_history_r[HISTORY_SIZE];
// extern uint16_t frame_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];

// ---------------------------------------------------------
// Function Prototypes
// ---------------------------------------------------------

uint16_t dim_pixel(uint16_t color, uint16_t divide);
void draw_line_hot(int x0, int y0, int x1, int y1, uint16_t color);
void draw_lissajous(void);
void draw_lissajous_connected(void);

#endif // LISSAJOUS_H