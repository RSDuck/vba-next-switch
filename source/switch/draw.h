/*
 *   Copyright 2017-2018 nx-hbmenu Authors
 *
 *   Permission to use, copy, modify, and/or distribute this software for any purpose
 *   with or without fee is hereby granted, provided that the above copyright notice
 *   and this permission notice appear in all copies.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 *   REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 *   FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT,
 *   OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 *   OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 *   ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H
#include "types.h"
#include <switch.h>

#define FONT_FACES_MAX PlSharedFontType_Total

#define font24 7
#define font16 5
#define font14 4

extern u8* currentFB;
extern u32 currentFBWidth;
extern u32 currentFBHeight;

// the following code is from nx-hbmenu
// https://github.com/switchbrew/nx-hbmenu/blob/master/common/common.h#L63

static inline u8 BlendColor(u32 src, u32 dst, u8 alpha) {
	u8 one_minus_alpha = (u8)255 - alpha;
	return (dst * alpha + src * one_minus_alpha) / (u8)255;
}

static inline color_t MakeColor(u8 r, u8 g, u8 b, u8 a) {
	color_t clr;
	clr.r = r;
	clr.g = g;
	clr.b = b;
	clr.a = a;
	return clr;
}

static inline void DrawPixel(u32 x, u32 y, color_t clr) {
	if (x >= 1280 || y >= 720) return;
	u32 off = (y * currentFBWidth + x) * 4;
	currentFB[off] = BlendColor(currentFB[off], clr.r, clr.a);
	off++;
	currentFB[off] = BlendColor(currentFB[off], clr.g, clr.a);
	off++;
	currentFB[off] = BlendColor(currentFB[off], clr.b, clr.a);
	off++;
	currentFB[off] = 0xff;
}

static inline void Draw4PixelsRaw(u32 x, u32 y, color_t clr) {
	if (x >= 1280 || y >= 720 || x > 1280 - 4) return;

	u32 color = clr.r | (clr.g << 8) | (clr.b << 16) | (0xff << 24);
	u128 val = color | ((u128)color << 32) | ((u128)color << 64) | ((u128)color << 96);
	u32 off = (y * currentFBWidth + x) * 4;
	*((u128*)&currentFB[off]) = val;
}

Result textInit(void);
bool fontInit(void);
void fontExit();

void drawRect(u32 x, u32 y, u32 w, u32 h, color_t color);
void drawRectRaw(u32 x, u32 y, u32 w, u32 h, color_t color);
void drawPixel(u32 x, u32 y, color_t clr);
void drawText(u32 font, u32 x, u32 y, color_t clr, const char* text);
void drawTextTruncate(u32 font, u32 x, u32 y, color_t clr, const char* text, u32 max_width, const char* end_text);
void getTextDimensions(u32 font, const char* text, u32* width_out, u32* height_out);