#ifndef SB_UTIL_H
#define SB_UTIL_H

#include "global_vars.h"

/* Firmware Includes */
#include "lib/dac/dac.h"
#include "lib/adc/adc.h"
#include "lib/led_driver/led_driver.h"
#include "lib/display/display.h"
#include "lib/buttons/buttons.h"
#include "lib/pot/pot.h"
#include "lib/codec/vs1053.h"

/* ======== Filehelper =======*/
uint32_t syncsafe_to_uint(const uint8_t *b);
void read_text_frame(FIL *fil, uint32_t frame_size, char *out, size_t out_size);
uint32_t find_audio_start(FIL *fil);
void get_mp3_header(FIL *fil, track_info_t *track);
void get_mp3_metadata(const char *filename, track_info_t *track);
int compare_filenames(const void *a, const void *b);

/* ======== Init ==============*/
void sb_hw_init(vs1053_t *player, st7789_t *display);

/* ========= Audio ========= */
void sb_audio_init(vs1053_t *player);

/* ========= MP3 / Metadata ========= */
int  sb_scan_tracks(track_info_t *tracks, int max_tracks);
void sb_print_track(track_info_t *t);

/* ========= Playback ========= */
// int sb_play_track(vs1053_t *player, track_info_t *track, st7789_t *display);
int jukebox(vs1053_t *player, track_info_t *track, st7789_t *display);


/* ====== Core 1 Entry  ======*/
void dprint(char * str, ...);

void core1_entry();

void update_scope_core1();
static void process_audio_batch();
void addIcons(uint16_t* frame_buffer, bool enabled);

/* ========= Display ========= */
// void fast_drawline(int x, int y1, int y2, uint16_t color); // Unused?

// ADD THIS LINE HERE:
// void update_scope_core1(void);

#ifndef PI
#define PI 3.141592653589793
#endif

/* ========= sb_util.c ========== */

void process_image(track_info_t *track, const char *filename, float output_size);
static void process_audio_batch();

void set_visualizer(int num);
void clear_framebuffer();
int get_selected_band();

#endif