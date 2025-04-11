#include "txt_writer.h"
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

bool createNewFile(const char* filepath) {
    if (!filepath) {
        fprintf(stderr, "Invalid filepath provided.\n");
        return false;
    }

    // Duplicate the filepath for safe modification
    char* path = strdup(filepath);
    if (!path) {
        perror("Error duplicating filepath");
        return false;
    }

    // Extract the directory path
    char* last_slash = strrchr(path, '/');
    if (last_slash) {
        *last_slash = '\0';  // Terminate at the last slash to isolate the directory path

        // Recursively create the directory path if it doesn't exist
        char* temp_path = strdup(path);
        if (!temp_path) {
            perror("Error duplicating path for directory creation");
            free(path);
            return false;
        }

        for (char* p = temp_path; *p; ++p) {
            if (*p == '/') {
                *p = '\0';  // Temporarily terminate the string to test this subpath
                if (strlen(temp_path) > 0 && mkdir(temp_path, 0755) != 0 && errno != EEXIST) {
                    perror("Error creating directory");
                    free(path);
                    free(temp_path);
                    return false;
                }
                *p = '/';  // Restore the slash
            }
        }

        // Create the final directory if it doesn't exist
        if (mkdir(temp_path, 0755) != 0 && errno != EEXIST) {
            perror("Error creating directory");
            free(path);
            free(temp_path);
            return false;
        }

        free(temp_path);
    }

    free(path);

    // Open the file in write mode
    FILE* file = fopen(filepath, "w");
    if (!file) {
        perror("Error creating file");
        return false;
    }

    fclose(file);  // Immediately close the file to ensure it's created
    return true;
}

bool writeLineToFile(char* filename, char* line) {
    FILE* file = fopen(filename, "a");  // Open file in append mode
    if (!file) {
        perror("Error opening file");
        return false;
    }

    fprintf(file, "%s\n", line);  // Write the line followed by a newline
    fclose(file);  // Close the file
    return true;
}

