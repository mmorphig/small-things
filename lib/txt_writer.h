#ifndef FILE_WRITER_H
#define FILE_WRITER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Write a single line to a given file
// Returns false on fail
bool writeLineToFile(char* filename, char* line);

// Creates a file, 'nuff said
bool createNewFile(const char* filepath);

#endif // FILE_READER_H
