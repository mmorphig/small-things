#include <stdio.h>
#include <stdlib.h>
#include "bmp_reader.h"

Color* readBMPToArray(char* filename, int* width, int* height) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Cannot open file %s\n", filename);
        return NULL;
    }

    unsigned char info[54];
    fread(info, sizeof(unsigned char), 54, file);

    *width = *(int*)&info[18];
    *height = *(int*)&info[22];

    int size = 3 * (*width) * (*height);
    unsigned char* data = (unsigned char*)malloc(size);
    fread(data, sizeof(unsigned char), size, file);
    fclose(file);

    int** arrayOut = (int**)malloc(*height * sizeof(int*));
    for (int i = 0; i < *height; i++) {
        arrayOut[i] = (int*)malloc(*width * sizeof(int));
        for (int j = 0; j < *width; j++) {
            int index = 3 * (i * (*width) + j);
            unsigned char r = data[index + 2];
            unsigned char g = data[index + 1];
            unsigned char b = data[index];
            int mirroredIndex = *width - 1 - j;
            // Set the current index's values
            arrayOut[i][mirroredIndex] = (Color){r, g, b, 0xFF};
            // Alpha value is at its highest
        }
    }

    free(data);
    return arrayOut;
}
