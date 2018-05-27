#ifndef __UTIL_H__
#define __UTIL_H__

#include <vector>

void addString(char* filenameBuffer, char** filenames, int* filenamesCount, char** nextFilename, const char* string);

bool isDirectory(char* path);
void getDirectoryContents(char* filenameBuffer, char** filenames, int* filenamesCount, const char* directory, const char* extensionFilter);

#include <string.h>

/*
    Usually I don't write comments to explain my functions, but since is a "safe" function,
    it's probably the best idea to do this "safe" thing from start to finish.

    Why do I write such a long text? Because it's simply a bit frustrating that there's 
    not such a function available in the standard library.

    Description:
	Writes the string stored at the point src is pointing to dst, including the null-terminating
	character. If src contains more bytes than dst can store, the copying is aborted on the character
	next to the last. The last character dst can hold is then used to store the string terminator.

    Arguments:
	dst_length contains the amount of bytes dst can hold including the null-terminating character.

    Short form:
    Use this instead of strcpy or strncpy. Put the length of dst in dst_length.

    TODO: add Unicode/UTF-8 support

*/
void strcpy_safe(char* dst, const char* src, unsigned dst_length);

#endif