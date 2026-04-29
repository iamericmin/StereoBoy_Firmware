/*
	"font.h", Written by Daniel C. MIT License.

	This file is C89.

	Compiler/preprocessor configs (optional):
	-D NO_LOWERCASE
	-D NO_SYMBOL
	-D NO_NUMBERS
	In case you are limited in space :)
*/
#ifndef FONT_H
#define FONT_H

#include "stdbool.h"
// #include "font_font.h"

#define font_width 11
#define font_height 20

// struct Font;
struct Font {
    char letter;
    bool code[font_width*font_height];
	// char width;
	// char height;
};

const struct Font * find_font_char(char c);
extern struct Font font[];

#endif