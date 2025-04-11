#ifndef BMP_READER_H
#define BMP_READER_H

#include <stdio.h>
#include <stdlib.h>

// Function to read a BMP file and write its pixel data to a 2D array
Color* readBMPToRGBArray(char* filename, int* width, int* height);

// Any use must use free()

typedef struct {
	uint8_t r, g, b;
	uint8_t a;
} Color; // Basically SDL_Color, but this to not depend on it

#endif // BMP_READER_H