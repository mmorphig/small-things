#ifndef PAUSE_MENU_H
#define PAUSE_MENU_H

#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

#define SCREEN_WIDTH 2200
#define SCREEN_HEIGHT 1400
#define WINDOW_WIDTH 2200
#define WINDOW_HEIGHT 1400

#define TEXT_SCALE 6

// Cursed global settings structure
typedef struct {
	int value;
	int is_dragging; // If it's a slider, this tracks if the user is sliding it
	int min_value, max_value;
} Setting;

typedef struct {
    Setting bumpscosity;
} Settings;

extern Settings settings;  // Declare the global settings

// Function to draw the pause menu onto the pixel buffer
void draw_pause_menu(unsigned char* pixelsm, SDL_Event* event);

// The funny function
void crash();

#endif // PAUSE_MENU_H
