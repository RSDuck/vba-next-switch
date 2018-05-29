#pragma once

#include <stdint.h>

#include <switch.h>

struct Image {
	u8* data;
	int width, height;
};

void imageLoad(Image* image, const char* filename);
void imageDeinit(Image* image);

void imageDraw(u8* fb, u16 fbWidth, u16 fbHeight, Image* image, int x, int y, int u = 0, int v = 0, int imageWidth = 0,
	       int imageHeight = 0);