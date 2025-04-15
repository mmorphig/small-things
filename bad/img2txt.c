#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define ASCII " .:-=+*%@#"  // Dark to bright ASCII characters
#define OUTPUT_FILE "ascii_output.txt"  // Output file for ASCII frames

#define MAX_FILES 30000

// Convert an image to BMP format
void convert_to_bmp(const char *input, const char *output, int height) {
    char command[256];

    // Extract image dimensions using ffprobe
    char size_cmd[256];
    snprintf(size_cmd, sizeof(size_cmd),
        "ffprobe -v error -select_streams v:0 -show_entries stream=width,height "
        "-of csv=p=0 \"%s\"", input);
    
    FILE *size_fp = popen(size_cmd, "r");
    if (!size_fp) {
        printf("Error: Could not get image dimensions for %s\n", input);
        return;
    }

    int img_width = 0, img_height = 0;
    if (fscanf(size_fp, "%d,%d", &img_width, &img_height) != 2) {
        printf("Error: Failed to read image size.\n");
        pclose(size_fp);
        return;
    }
    pclose(size_fp);

    // Compute new width using aspect ratio & character aspect ratio (~1.7)
    int width = (int)((img_width / (double)img_height) * height * 1.7);
    if (width < 1) width = 1;  // Ensure a minimum valid width

    // Convert image using FFmpeg
    snprintf(command, sizeof(command),
        "ffmpeg -y -i \"%s\" -vf \"scale=%d:%d,format=bgr24\" \"%s\" -loglevel error",
        input, width, height, output);
    
    int ret = system(command);
    if (ret != 0) {
        printf("[ERROR] FFmpeg failed for %s\n", input);
    }
}

// Read a BMP file into a 2D grayscale array
void read_bmp(const char *filename, int **image, int *width, int *height) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Cannot open file %s\n", filename);
        return;
    }

    unsigned char info[54];
    fread(info, sizeof(unsigned char), 54, file);

    *width = *(int*)&info[18];
    *height = *(int*)&info[22];

    int row_padded = (*width * 3 + 3) & (~3);
    unsigned char* data = (unsigned char*)malloc(row_padded * (*height));
    fread(data, sizeof(unsigned char), row_padded * (*height), file);
    fclose(file);

    *image = (int*)malloc((*width) * (*height) * sizeof(int));
	if (!(*image)) {
	    printf("Error: Memory allocation failed for %s\n", filename);
	    free(data);
	    return;
	}

    // Convert RGB to grayscale
    for (int y = 0; y < *height; y++) {
        for (int x = 0; x < *width; x++) {
            int index = ((*height - 1 - y) * row_padded) + (x * 3);
            unsigned char r = data[index + 2];
            unsigned char g = data[index + 1];
            unsigned char b = data[index];
            int gray = (r + g + b) / 3;
            (*image)[y * (*width) + x] = gray;
        }
    }
    free(data);

    // Dithering
    for (int y = 0; y < *height - 1; y++) {
        for (int x = 1; x < *width - 1; x++) {
            int index = y * (*width) + x;
            int old_pixel = (*image)[index];
            int new_pixel = (old_pixel * (strlen(ASCII) - 1)) / 255 * (255 / (strlen(ASCII) - 1));
            (*image)[index] = new_pixel;

            int quant_error = old_pixel - new_pixel;

            (*image)[index + 1] += (quant_error * 7) / 16;   // Right
            (*image)[index + (*width) - 1] += (quant_error * 3) / 16; // Bottom-left
            (*image)[index + (*width)] += (quant_error * 5) / 16;     // Bottom
            (*image)[index + (*width) + 1] += (quant_error * 1) / 16; // Bottom-right
        }
    }
}

void append_ascii_to_file(int *image, int width, int height) {
    FILE *file = fopen(OUTPUT_FILE, "a");
    if (!file) {
        printf("Error: Could not open %s\n", OUTPUT_FILE);
        return;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int gray = (image[y * width + x] * (strlen(ASCII) - 1)) / 255;
            fputc(ASCII[gray], file);
        }
        fputc('\n', file);
    }
    fputc('\n', file);
    fclose(file);
}

// Extracts the first number in a filename
int extract_number(const char *filename) {
    while (*filename && !isdigit(*filename)) filename++; // Skip non-digits
    return atoi(filename); // Convert the first found number
}

// Compare function for qsort() to sort numerically
int compare_filenames(const void *a, const void *b) {
    int num1 = extract_number(*(const char **)a);
    int num2 = extract_number(*(const char **)b);
    return num1 - num2;
}

void process_images(const char *dir_path, int height) {
    struct dirent *entry;
    DIR *dir = opendir(dir_path);
    
    if (!dir) {
        printf("Error: Could not open directory %s\n", dir_path);
        return;
    }

    // Use heap allocation to avoid stack overflow
    char **filenames = malloc(MAX_FILES * sizeof(char *));
    if (!filenames) {
        printf("Error: Memory allocation failed for filenames array.\n");
        closedir(dir);
        return;
    }

    int file_count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".png") || strstr(entry->d_name, ".jpg")) {
            if (file_count >= MAX_FILES) {
                printf("Warning: Reached max file limit (%d), skipping extra files.\n", MAX_FILES);
                break;
            }
            filenames[file_count] = strdup(entry->d_name);
            if (!filenames[file_count]) {
                printf("Error: Memory allocation failed for filename %s.\n", entry->d_name);
                break;
            }
            file_count++;
        }
    }
    closedir(dir);

    qsort(filenames, file_count, sizeof(char *), compare_filenames);

    FILE *file = fopen(OUTPUT_FILE, "w");
    if (file) fclose(file);

    char filepath[512], bmpfile[] = "temp.bmp";

    for (int i = 0; i < file_count; i++) {
        snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, filenames[i]);

        free(filenames[i]);  // Free after use

        convert_to_bmp(filepath, bmpfile, height);

        int *image = NULL;
        int width, actual_height;
        read_bmp(bmpfile, &image, &width, &actual_height);

        if (image) {
            append_ascii_to_file(image, width, actual_height);
            free(image);
        }

        printf("%d/%d: %s processed\n", i + 1, file_count, filepath);

        // Batch processing: Flush output every 1000 files
        if (i % 1000 == 0) {
            fclose(fopen(OUTPUT_FILE, "a"));  
        }
    }

    free(filenames);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <image_folder> <height>\n", argv[0]);
        return 1;
    }

    int height = atoi(argv[2]);
    if (height <= 0) {
        printf("Error: Height must be a positive integer.\n");
        return 1;
    }

    process_images(argv[1], height);
    return 0;
}

