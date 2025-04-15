#ifndef MP3_PLAYER_H
#define MP3_PLAYER_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "mp3_data.h"

// Function to play MP3 using FFmpeg from memory
void *play_mp3() {
	FILE *ffmpeg_pipe = popen("ffmpeg -i pipe:0 -f alsa default -loglevel quiet -hide_banner >/dev/null 2>&1", "w");
    if (!ffmpeg_pipe) {
        fprintf(stderr, "[ERROR] Failed to open FFmpeg pipe!\n");
        return NULL;
    }

    fwrite(mp3_data, 1, mp3_size, ffmpeg_pipe);
    fclose(ffmpeg_pipe);
    
    return NULL;
}

// Start MP3 playback in a separate thread
void start_mp3_player() {
    pthread_t mp3_thread;
    pthread_create(&mp3_thread, NULL, play_mp3, NULL);
}

#endif // MP3_PLAYER_H
