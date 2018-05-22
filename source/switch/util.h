#ifndef __UTIL_H__
#define __UTIL_H__

#include <vector>

void addString(char* filenameBuffer, char** filenames, int* filenamesCount, char** nextFilename, const char* string);

bool isDirectory(char* path);
void getDirectoryContents(char* filenameBuffer, char** filenames, int* filenamesCount, const char* directory, const char* extensionFilter);

#endif