#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "system.h"
#include "port.h"
#include "types.h"
#include "util.h"

#include "gba.h"
#include "globals.h"
#include "memory.h"

extern bool cpuIsMultiBoot;

bool utilIsGBAImage(const char * file)
{
	cpuIsMultiBoot = false;
	if(strlen(file) > 4)
	{
		const char * p = strrchr(file,'.');

		if(p != NULL)
		{
			if((strcasecmp(p, ".agb") == 0) ||
					(strcasecmp(p, ".gba") == 0) ||
					(strcasecmp(p, ".bin") == 0) ||
					(strcasecmp(p, ".elf") == 0))
				return true;
			if(strcasecmp(p, ".mb") == 0)
			{
				cpuIsMultiBoot = true;
				return true;
			}
		}
	}

	return false;
}

static int utilGetSize(int size)
{
	int res = 1;

	while(res < size)
		res <<= 1;

	return res;
}

uint8_t *utilLoad(const char *file, bool (*accept)(const char *), uint8_t *data, int &size)
{
	FILE *fp = NULL;

	fp = fopen(file,"rb");
	fseek(fp, 0, SEEK_END); /*go to end*/
	size = ftell(fp); /* get position at end (length)*/
	rewind(fp);

	uint8_t *image = data;
	if(image == NULL)
	{
		/*allocate buffer memory if none was passed to the function*/
		image = (uint8_t *)malloc(utilGetSize(size));
		if(image == NULL)
		{
			systemMessage("Failed to allocate memory for data");
			return NULL;
		}
	}

	fread(image, 1, size, fp); /* read into buffer*/
	fclose(fp);
	return image;
}

/* Not endian safe, but VBA itself doesn't seem to care */
void utilWriteIntMem(uint8_t *& data, int val)
{
	memcpy(data, &val, sizeof(int));
	data += sizeof(int);
}

void utilWriteMem(uint8_t *& data, const void *in_data, unsigned size)
{
	memcpy(data, in_data, size);
	data += size;
}

void utilWriteDataMem(uint8_t *& data, variable_desc *desc)
{
	while (desc->address) 
	{
		utilWriteMem(data, desc->address, desc->size);
		desc++;
	}
}

int utilReadIntMem(const uint8_t *& data)
{
	int res;

	memcpy(&res, data, sizeof(int));
	data += sizeof(int);
	return res;
}

void utilReadMem(void *buf, const uint8_t *& data, unsigned size)
{
	memcpy(buf, data, size);
	data += size;
}

void utilReadDataMem(const uint8_t *& data, variable_desc *desc)
{
	while (desc->address)
	{
		utilReadMem(desc->address, data, desc->size);
		desc++;
	}
}
