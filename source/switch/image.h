#pragma once

#include <stdint.h>

#include <switch/types.h>

struct Image {
	u8* data;
	int width, height;
};

void imageLoad(Image* image, const char* filename);
void imageDeinit(Image* image);

void imageDraw(Image* image, int x, int y, int _alpha = 255, int u = 0, int v = 0, int imageWidth = 0, int imageHeight = 0);