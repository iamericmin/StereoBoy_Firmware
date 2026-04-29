#include "album_art.h"
#include "fft.h"
#include "lissajous.h"

#define WAVE_L_COLOR 0x059f
#define WAVE_R_COLOR 0xffff
#define FFT_L_COLOR_DARK 0x0600
#define FFT_R_COLOR_DARK 0x05FF
#define FFT_L_COLOR_LIGHT 0x8FF1
#define FFT_R_COLOR_LIGHT 0xAFFF


// Display and oscope stuff
// #define WAVE_COLOR 0x07E0 // Bright Green
#define BG_COLOR 0x0000   // Black

// Updated Constants for Split View
#define TARGET_HEIGHT 60 // Height of each individual wave (reduced to prevent overlap)

#define OFFSET_L 150 // Bottom half-ish
#define OFFSET_R 90  // Top half-ish