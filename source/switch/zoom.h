#pragma once

#include <stdint.h>
#include <stdio.h>

/*
    Taken from SDL_gfx

*/

typedef struct {
	int w, h, pitch;
	uint32_t *pixels;
} Surface;

typedef struct { uint8_t r, g, b, a; } ColorRGBA;

void zoomInit(int maxWidth, int maxHeight);
void zoomDeinit();

// don't use these from multiple threads, they share the same temp buffers
int zoomSurfaceRGBA(Surface *src, Surface *dst, int flipx, int flipy, int smooth);

void zoomResizeBilinear_RGB8888(uint8_t *dst, uint32_t dst_width, uint32_t dst_height, uint8_t *src, uint32_t src_width,
				     uint32_t src_height, uint32_t src_stride);