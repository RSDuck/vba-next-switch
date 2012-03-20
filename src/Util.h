#ifndef UTIL_H
#define UTIL_H

#include "System.h"

enum {
	IMAGE_UNKNOWN,
	IMAGE_GBA
};

// save game
typedef struct {
	void *address;
	int size;
} variable_desc;

bool utilIsGBAImage(const char *);
void utilStripDoubleExtension(const char *, char *);
uint32_t utilFindType(const char *);
uint8_t *utilLoad(const char *, bool (*)(const char*), uint8_t *, int &);

void utilPutDword(uint8_t *, uint32_t);
void utilPutWord(uint8_t *, uint16_t);

void utilGBAFindSave(const uint8_t *, const int);

void utilUpdateSystemColorMaps();
bool utilFileExists( const char *filename );

void utilWriteIntMem(uint8_t *& data, int);
void utilWriteMem(uint8_t *& data, const void *in_data, unsigned size);
void utilWriteDataMem(uint8_t *& data, variable_desc *);

int utilReadIntMem(const uint8_t *& data);
void utilReadMem(void *buf, const uint8_t *& data, unsigned size);
void utilReadDataMem(const uint8_t *& data, variable_desc *);

#endif // UTIL_H
