#ifndef FFT_H
#define FFT_H

#include <complex.h>
#include <stdint.h>
#include <math.h>


typedef float complex cplx;

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240
#define FFT_L_COLOR_DARK  0x0600
#define FFT_R_COLOR_DARK  0x05FF
#define FFT_L_COLOR_LIGHT 0x8FF1
#define FFT_R_COLOR_LIGHT 0xAFFF

void bit_reverse(cplx buf[], int n);
void fft_optimized(cplx buf[], int n);
void draw_bins(int n);
void draw_spectrum_bars(int x_start, int width, int h_l, int h_r, int target_l, int target_r);

#endif // FFT_H