#include "txt_reader.h"
#include <string.h>

char** fileTo2dArray(const char* filename, size_t* rows, size_t* cols) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Could not open file");
        return NULL;
    }

    size_t buffer_size = 1024;
    char buffer[buffer_size];
    *rows = 0;
    *cols = 0;

    // First pass to count the number of rows and find the longest line
    while (fgets(buffer, buffer_size, file)) {
        (*rows)++;
        size_t line_length = strlen(buffer);
        if (line_length > *cols) {
            *cols = line_length;  // Update the maximum column length
        }
    }

    // Allocate memory for the 2D array
    char** array = (char**)malloc(*rows * sizeof(char*));
    if (!array) {
        perror("Memory allocation failed for rows");
        fclose(file);
        return NULL;
    }

    // Reset the file pointer to the beginning of the file
    rewind(file);

    // Second pass to populate the 2D array
    size_t row = 0;
    while (fgets(buffer, buffer_size, file)) {
        // Allocate memory for each row, plus one for the null-terminator
        array[row] = (char*)malloc((*cols + 1) * sizeof(char));
        if (!array[row]) {
            perror("Memory allocation failed for a row");
            free2dArray(array, row);  // Free the rows allocated so far
            fclose(file);
            return NULL;
        }

        // Copy the line into the array (use strncpy to avoid buffer overflows)
        strncpy(array[row], buffer, *cols);
        array[row][*cols] = '\0';  // Null-terminate each line
        row++;
    }

    fclose(file);
    return array;
}

void free2dArray(char** array, size_t rows) {
    for (size_t i = 0; i < rows; i++) {
        free(array[i]);  // Free each row
    }
    free(array);  // Free the main array
}

// Helper function to read a specific line from a file
char* readLine(FILE* file, int line_number) {
    char* buffer = NULL;
    size_t bufsize = 0;
    int current_line = 0;

    // Read through the file line by line
    while (getline(&buffer, &bufsize, file) != -1) {
        current_line++;
        if (current_line == line_number) {
            return buffer;  // Return the line when found
        }
    }

    // Line not found, free the buffer and return NULL
    free(buffer);
    return NULL;
}

// Function to read a specific line from a file and return it as a dynamically allocated array of characters
char* readLineFromFile(const char* filename, int line_number) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        return NULL;  // Error opening file
    }

    // Read the specified line
    char* line = readLine(file, line_number);
    fclose(file);

    if (!line) {
        return NULL;  // Error reading the line or line doesn't exist
    }

    // Return the line as a dynamically allocated array of characters
    return line;  // Caller is responsible for freeing this
}

// Function to count the total number of lines in a file
int countLinesInFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        return -1; // Return -1 on error
    }

    int lines = 0;
    char ch;

    // Count each newline character to count lines
    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\n') {
            lines++;
        }
    }

    fclose(file); // Close the file when done
    return lines;
}
