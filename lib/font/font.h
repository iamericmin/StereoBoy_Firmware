#ifndef FONT_H
#define FONT_H

#include <stdint.h>
#include <stddef.h>

extern const uint8_t font_width;
extern const uint8_t font_height;

struct Font {
    char letter;
    uint8_t code[220]; // 8-bit alpha intensity map (0-255)
};

extern const struct Font font[];
const struct Font* find_font_char(char c);

#endif // FONT_H
