#include "font.h"
#include "stddef.h"

const struct Font* find_font_char(char c) {
    for (int i = 0; font[i].letter != 0; i++) {
        if (font[i].letter == c) return &font[i];
    }
    return NULL;
}