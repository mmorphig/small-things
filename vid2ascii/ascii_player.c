#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "ascii_data.h"  // ASCII animation frames + FPS
#include "mp3_player.h"  // MP3 playback

int FRAME_DELAY;  // Microseconds per frame

void play_ascii_animation() {
    struct timespec start, end;
    long frame_time, sleep_time;
    int frame_counter = 0;
    struct timespec fps_start;
    
    clock_gettime(CLOCK_MONOTONIC, &fps_start); // Track FPS time window

    printf("\033[H"); // Move cursor to top-left without clearing
    for (int i = 0; i < frame_count; i++) {
        clock_gettime(CLOCK_MONOTONIC, &start); // Start timing frame

        printf("\033[H"); // Reset cursor position
        printf("%s\n", frames[i]); // Print frame
        fflush(stdout); // Ensure output is immediately updated

        clock_gettime(CLOCK_MONOTONIC, &end); // End timing frame

        // Calculate elapsed time for frame rendering
        frame_time = (end.tv_sec - start.tv_sec) * 1000000L + 
                     (end.tv_nsec - start.tv_nsec) / 1000L;

        // Adjust sleep to maintain correct FPS
        sleep_time = FRAME_DELAY - frame_time;
        if (sleep_time > 0) {
            usleep(sleep_time);
        }

        // FPS Tracking: Print every 2 seconds
        frame_counter++;
        if (frame_counter % (int) ascii_fps == 0) {  // Every second
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            double elapsed = (now.tv_sec - fps_start.tv_sec) + 
                             (now.tv_nsec - fps_start.tv_nsec) / 1e9;
            if (elapsed >= 2.0) {
                double fps = frame_counter / elapsed;
                printf("\033[2;0H[INFO] FPS: %.2f\n", fps); // Print FPS at fixed position
                fps_start = now;  // Reset FPS timer
                frame_counter = 0;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    int loop = 0;

    if (argc > 1) {
        if (strcmp(argv[1], "l") == 0) {
            loop = 1;
        }
    }

    // Compute frame delay from FPS
    FRAME_DELAY = 1000000 / ascii_fps;

    printf("\033[H\033[J"); // Clear screen

    start_mp3_player(); // Start MP3 playback

    play_ascii_animation(); // Play once
    while (loop) { // Loop animation forever
        play_ascii_animation();
    }

    printf("\033[H\033[J"); // Clear screen
    return 0;
}
