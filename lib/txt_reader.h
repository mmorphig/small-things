#ifndef FILE_READER_H
#define FILE_READER_H

#include <stdio.h>
#include <stdlib.h>

// Function to read a file into a dynamically allocated 2D array
// filename: the name of the file to read
// rows: pointer to store the number of rows
// cols: pointer to store the maximum number of columns (longest line length)
// Returns: pointer to a dynamically allocated 2D array, or NULL on failure
char** fileTo2dArray(const char* filename, size_t* rows, size_t* cols);

// Function to free the dynamically allocated 2D array
void free2dArray(char** array, size_t rows);

// Function to read a specific line from a file
char* readLineFromFile(const char* filename, int line_number);

// Function to count the total number of lines in a file
int countLinesInFile(const char* filename);

#endif // FILE_READER_H
