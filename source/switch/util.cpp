#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dirent.h>

#include "util.h"

struct alphabetize {
	inline bool operator()(char* a, char* b) { return strcasecmp(a, b) < 0; }
};

static int sortAlpha(const void* a, const void* b) { return strcasecmp(*((const char**)a), *((const char**)b)); }

bool isDirectory(char* path) {
	DIR* dir = opendir(path);
	if (!dir) {
		return false;
	}

	closedir(dir);
	return true;
}

void addString(char* filenameBuffer, char** filenames, int* filenamesCount, char** nextFilename, const char* string) {
	filenames[(*filenamesCount)++] = *nextFilename;
	strcpy(*nextFilename, string);
	*nextFilename += strlen(string) + 1;
}

void getDirectoryContents(char* filenameBuffer, char** filenames, int* filenamesCount, const char* directory, const char* extensionFilter) {
	char slash[strlen(directory) + 2];
	snprintf(slash, sizeof(slash), "%s/", directory);
	DIR* dir = opendir(slash);

	int maxFilenamesCount = *filenamesCount;

	char* nextFilename = filenameBuffer;
	*filenamesCount = 0;

	addString(filenameBuffer, filenames, filenamesCount, &nextFilename, "..");

	if (dir != NULL) {
		for (int i = 0; i < maxFilenamesCount; i++) {
			struct dirent* ent = readdir(dir);
			if (ent == NULL) {
				break;
			}

			char path[strlen(directory) + strlen(ent->d_name) + 2];
			snprintf(path, sizeof(path) / sizeof(char), "%s/%s", directory, ent->d_name);
			if (isDirectory(path)) {
				addString(filenameBuffer, filenames, filenamesCount, &nextFilename, ent->d_name);
			} else {
				const char* dot = strrchr(path, '.');
				if (dot && dot != path && strcmp(dot + 1, extensionFilter) == 0) {
					addString(filenameBuffer, filenames, filenamesCount, &nextFilename, ent->d_name);
				}
			}
		}

		closedir(dir);
	}
	qsort(filenames + 1, (*filenamesCount) - 1, sizeof(char*), &sortAlpha);
}

void strcpy_safe(char* dst, const char* src, unsigned src_length) {
	unsigned i = 0;
	while (src[i] != '\0' && i < src_length - 2) {
		dst[i] = src[i];
		i++;
	}
	dst[i] = '\0';
}
