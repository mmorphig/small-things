#include <SDL2/SDL.h>
#include <pthread.h>
#include "pause_menu.h"

// Initialize the global settings
Settings settings = {
    .bumpscosity.value = 10,
    .bumpscosity.is_dragging = 0,
    .bumpscosity.max_value = 1000,
    .bumpscosity.min_value = 0
};

// 5x7 pixel font for each letter in byte notation
unsigned char font[36][7] = {
    {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001},  // 'A'
    {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110},  // 'B'
    {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110},  // 'C'
    {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110},  // 'D'
    {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111},  // 'E'
    {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000},  // 'F'
    {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01110},  // 'G'
    {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001},  // 'H'
    {0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110},  // 'I'
    {0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100},  // 'J'
    {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001},  // 'K'
    {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111},  // 'L'
    {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001},  // 'M'
    {0b10001, 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001},  // 'N'
    {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110},  // 'O'
    {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000},  // 'P'
    {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101},  // 'Q'
    {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001},  // 'R'
    {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110},  // 'S'
    {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100},  // 'T'
    {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110},  // 'U'
    {0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b01010, 0b00100},  // 'V'
    {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b11011, 0b10001},  // 'W'
    {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001},  // 'X'
    {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100},  // 'Y'
    {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111},  // 'Z'
    {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110},  // '0'
    {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110},  // '1'
    {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111},  // '2'
    {0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110},  // '3'
    {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010},  // '4'
    {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110},  // '5'
    {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110},  // '6'
    {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000},  // '7'
    {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110},  // '8'
    {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100}   // '9'
};

// Function to set a pixel color in the buffer
static void set_pixel(unsigned char* pixels, int x, int y, unsigned char r, unsigned char g, unsigned char b) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return;
    int index = (y * SCREEN_WIDTH + x) * 3;
    pixels[index] = r;
    pixels[index + 1] = g;
    pixels[index + 2] = b;
}

// Function to draw a filled rectangle (for backgrounds, etc.)
static void draw_rect(unsigned char* pixels, int x, int y, int width, int height, unsigned char r, unsigned char g, unsigned char b) {
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            set_pixel(pixels, x + i, y + j, r, g, b);
        }
    }
}

void draw_slider(unsigned char* pixels, int x, int y, int width, int height, SDL_Event* event, Setting* setting) {
    const int handle_width = 10;  // Width of slider handle

    // Draw slider track (a horizontal bar)
    draw_rect(pixels, x, y + height / 2 - 2, width, 4, 150, 150, 150); // Light gray track

    // Calculate handle position based on setting->value
    int handle_x = x + ((setting->value - setting->min_value) * (width - handle_width)) / (setting->max_value - setting->min_value);
    draw_rect(pixels, handle_x, y, handle_width, height, 200, 200, 200); // White handle

    // Handle mouse input
    if (event) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);

        if (event->type == SDL_MOUSEBUTTONDOWN) {
            int flipped_y = SCREEN_HEIGHT - my; // Only needed if OpenGL has Y flipped
            // Check if mouse is anywhere on the slider bar
            if (mx >= x && mx <= x + width && flipped_y >= y && flipped_y <= y + height) {
                setting->is_dragging = 1;
                
                // Instantly move slider to mouse position when clicked
                setting->value = setting->min_value + ((mx - x) * (setting->max_value - setting->min_value)) / (width - handle_width);
            }
        }
        else if (event->type == SDL_MOUSEBUTTONUP) {
            setting->is_dragging = 0; // Stop dragging
        }
        else if (event->type == SDL_MOUSEMOTION && setting->is_dragging) {
            // Update slider position based on mouse movement
            setting->value = setting->min_value + ((mx - x) * (setting->max_value - setting->min_value)) / (width - handle_width);

            // Clamp value
            if (setting->value < setting->min_value) setting->value = setting->min_value;
            if (setting->value > setting->max_value) setting->value = setting->max_value;
        }
    }
}

// Function to draw a scaled character with variable scaling
static void draw_char(unsigned char* pixels, int x, int y, unsigned char c, int scale, unsigned char r, unsigned char g, unsigned char b) {
    if (((c < 'A' || c > 'Z') && (c < '0' || c > '9')) || scale < 1) return; // Only support uppercase letters and numbers

    unsigned char* char_map;
    
    if (c >= 'A' && c <= 'Z') {
        char_map = font[c - 'A']; // Get letter map
    } else {
        char_map = font[c - '0' + 26]; // Get number map (assuming numbers start at index 26)
    }

    // Scale the character dynamically
    for (int j = 0; j < 7; j++) {
        for (int i = 0; i < 5; i++) {
            if (char_map[6 - j] & (1 << (4 - i))) {  // Reverse row order to fix upside-down issue
                // Scale each pixel by the factor (scaling to a `scale x scale` block)
                for (int dy = 0; dy < scale; dy++) {
                    for (int dx = 0; dx < scale; dx++) {
                        set_pixel(pixels, x + i * scale + dx, y + j * scale + dy, r, g, b);
                    }
                }
            }
        }
    }
}

// Function to draw the pause menu
void draw_pause_menu(unsigned char* pixels, SDL_Event* event) {
    // Clear screen with a dark overlay
    draw_rect(pixels, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, 0); // Black background

    // Draw a semi-transparent rectangle in the center (Pause Menu Box)
    int box_width = 800, box_height = 1000;
    int box_x = (SCREEN_WIDTH - box_width) / 2;
    int box_y = (SCREEN_HEIGHT - box_height) / 2;
    draw_rect(pixels, box_x, box_y, box_width, box_height, 50, 50, 50); // Dark gray box

    // Define text and calculate width dynamically
    char* text = (char*)malloc(sizeof("PAUSED") * sizeof(char));
    strcpy(text, "PAUSED");
    int num_chars = strlen(text); // Exclude null terminator

    int char_width = 5 * TEXT_SCALE;  // Each character is 5 pixels wide, scaled
    int char_height = 7 * TEXT_SCALE; // Each character is 7 pixels tall, scaled
    int spacing = TEXT_SCALE;         // Space between characters (scaled)

    int text_width = num_chars * (char_width + spacing) - spacing; // Total width of the text
    int text_height = char_height; // Text height is just one row of characters

    // Calculate centered position
    int text_x = box_x + (box_width - text_width) / 2;
    int text_y = box_y + box_height - text_height - 20;

    // Draw the text
    for (int i = 0; i < num_chars; i++) {
        draw_char(pixels, text_x + i * (char_width + spacing), text_y, text[i], TEXT_SCALE, 255, 255, 255); // White text
    }
    
	// Draw the volume slider
    int slider_x = box_x + box_width/2;
    int slider_y = box_y + box_height - 100 - char_height;
    int slider_width = box_width - box_width/2 - 20;
    int slider_height = 20;
    
    char value_text[10]; // Buffer to hold the slider value text
    snprintf(value_text, sizeof(value_text), "%i", settings.bumpscosity.value); // Convert float to string (1 decimal place)

    int num_value_chars = strlen(value_text);

    // Draw the slider value text
    for (int i = 0; i < num_value_chars; i++) {
        draw_char(pixels, slider_x - (num_value_chars - i) * (char_width / 2 + spacing / 2) - 10, slider_y, 
                  value_text[i], TEXT_SCALE / 2, 255, 255, 255); // White text
    }

    // Define text and calculate width dynamically
    text = (char*)realloc(text, sizeof("BUMPSCOSITY") * sizeof(char));
    strcpy(text, "BUMPSCOSITY");
    num_chars = strlen(text); // Exclude null terminator
    // Draw the text
    for (int i = 0; i < num_chars; i++) {
        draw_char(pixels, box_x + 20 + i * (char_width/2 + spacing/2) - spacing, slider_y , text[i], TEXT_SCALE/2, 255, 255, 255); // White text
    }
    
	draw_slider(pixels, slider_x, slider_y, slider_width, slider_height, event, &settings.bumpscosity);

	//int crashing = 1;
    //crash(crashing);
    free(text);
}

/*void crash(int crashing) {
	int crashlst[5] = {0,0,0,0,0};
	int crashlst2 = crashlst[100];
	
	int* crashptr = NULL;
	free(crashptr);
	free(crashptr);
	
	char* crashstr = (char*)malloc(sizeof("lol") * sizeof(char));
	strcpy(crashstr, "According to all known laws of aviation, there is no way that a bee should be able to fly.\nIts wings are too small to get its fat little body off the ground.\nThe bee, of course, flies anyway.\nBecause bees don’t care what humans think is impossible.\n");
	free(crashstr);
	
	int x = 1 / 0; 
	
	if (crashing) {
		crashing = 0;
		#pragma omp parallel for
		for (int i = 0; i < 100; i++) {
			crash(crashing);
		}
		crashing = 1;
	}
	
	// Enough mucking about, kys
	printf("%s", crashstr);
	*crashptr = 42;
	
	return;
}*/
