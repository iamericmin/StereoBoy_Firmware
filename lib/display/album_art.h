#include "lib/sb_util/global_vars.h"

uint64_t generate_FNV(const char *title, const char *artist, const char *album);
uint32_t lookup_LUT(uint64_t hash);
bool load_LUT();
bool load_album_cover(uint16_t *img_buffer, uint32_t pointer);
void display_album_art(uint16_t *img_buffer, const char* artist, const char* album, const char* title);
void generate_tv_test_pattern(uint16_t *buffer);