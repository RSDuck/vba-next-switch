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

#include "draw.h"

#include <arm_neon.h>

static FT_Error s_font_libret = 1;
static FT_Error s_font_facesret[FONT_FACES_MAX];
static FT_Library s_font_library;
static FT_Face s_font_faces[FONT_FACES_MAX];
static FT_Face s_font_lastusedface;
static size_t s_font_faces_total = 0;
static u64 s_textLanguageCode = 0;

Result textInit(void) {
	Result res = setInitialize();
	if (R_SUCCEEDED(res)) {
		res = setGetSystemLanguage(&s_textLanguageCode);
	}
	setExit();
	return res;
}

static bool FontSetType(u32 scale) {
	FT_Error ret = 0;
	for (u32 i = 0; i < s_font_faces_total; i++) {
		ret = FT_Set_Char_Size(s_font_faces[i], 0, scale * 64, 300, 300);
		if (ret) return false;
	}
	return true;
}

static inline bool FontLoadGlyph(glyph_t* glyph, u32 font, u32 codepoint) {
	FT_Face face;
	FT_Error ret = 0;
	FT_GlyphSlot slot;
	FT_UInt glyph_index;
	FT_Bitmap* bitmap;

	if (s_font_faces_total == 0) return false;

	for (u32 i = 0; i < s_font_faces_total; i++) {
		face = s_font_faces[i];
		s_font_lastusedface = face;
		glyph_index = FT_Get_Char_Index(face, codepoint);
		if (glyph_index == 0) continue;

		ret = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
		if (ret == 0) {
			ret = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
		}
		if (ret) return false;

		break;
	}

	slot = face->glyph;
	bitmap = &slot->bitmap;

	glyph->width = bitmap->width;
	glyph->height = bitmap->rows;
	glyph->pitch = bitmap->pitch;
	glyph->data = bitmap->buffer;
	glyph->advance = slot->advance.x >> 6;
	glyph->posX = slot->bitmap_left;
	glyph->posY = slot->bitmap_top;
	return true;
}

static void DrawGlyph(u32 x, u32 y, color_t clr, const glyph_t* glyph) {
	const u8* data = glyph->data;
	x += glyph->posX;
	y -= glyph->posY;

	for (u32 j = 0; j < glyph->height; j++) {
		for (u32 i = 0; i < glyph->width; i++) {
			color_t modulatedColor = clr;
			modulatedColor.a = ((u32)data[i] * (u32)clr.a) / 255;
			if (!modulatedColor.a) continue;
			DrawPixel(x + i, y + j, modulatedColor);
		}
		data += glyph->pitch;
	}
}

static inline u8 DecodeByte(const char** ptr) {
	u8 c = (u8) * *ptr;
	*ptr += 1;
	return c;
}

// UTF-8 code adapted from http://www.json.org/JSON_checker/utf8_decode.c
static inline int8_t DecodeUTF8Cont(const char** ptr) {
	int c = DecodeByte(ptr);
	return ((c & 0xC0) == 0x80) ? (c & 0x3F) : -1;
}

static inline u32 DecodeUTF8(const char** ptr) {
	u32 r;
	u8 c;
	int8_t c1, c2, c3;

	c = DecodeByte(ptr);
	if ((c & 0x80) == 0) return c;
	if ((c & 0xE0) == 0xC0) {
		c1 = DecodeUTF8Cont(ptr);
		if (c1 >= 0) {
			r = ((c & 0x1F) << 6) | c1;
			if (r >= 0x80) return r;
		}
	} else if ((c & 0xF0) == 0xE0) {
		c1 = DecodeUTF8Cont(ptr);
		if (c1 >= 0) {
			c2 = DecodeUTF8Cont(ptr);
			if (c2 >= 0) {
				r = ((c & 0x0F) << 12) | (c1 << 6) | c2;
				if (r >= 0x800 && (r < 0xD800 || r >= 0xE000)) return r;
			}
		}
	} else if ((c & 0xF8) == 0xF0) {
		c1 = DecodeUTF8Cont(ptr);
		if (c1 >= 0) {
			c2 = DecodeUTF8Cont(ptr);
			if (c2 >= 0) {
				c3 = DecodeUTF8Cont(ptr);
				if (c3 >= 0) {
					r = ((c & 0x07) << 18) | (c1 << 12) | (c2 << 6) | c3;
					if (r >= 0x10000 && r < 0x110000) return r;
				}
			}
		}
	}
	return 0xFFFD;
}

static void drawText_(u32 font, u32 x, u32 y, color_t clr, const char* text, u32 max_width, const char* end_text) {
	u32 origX = x;
	if (s_font_faces_total == 0) return;
	if (!FontSetType(font)) return;
	s_font_lastusedface = s_font_faces[0];

	while (*text) {
		if (max_width && x - origX >= max_width) {
			text = end_text;
			max_width = 0;
		}

		glyph_t glyph;
		u32 codepoint = DecodeUTF8(&text);

		if (codepoint == '\n') {
			if (max_width) {
				text = end_text;
				max_width = 0;
				continue;
			}

			x = origX;
			y += s_font_lastusedface->size->metrics.height / 64;
			continue;
		}

		if (!FontLoadGlyph(&glyph, font, codepoint)) {
			if (!FontLoadGlyph(&glyph, font, '?')) continue;
		}

		DrawGlyph(x, y + font * 3, clr, &glyph);
		x += glyph.advance;
	}
}

void drawText(u32 font, u32 x, u32 y, color_t clr, const char* text, ...) {
	char buffer[256];
	va_list args;
	va_start(args, text);
	vsnprintf(buffer, sizeof(buffer) / sizeof(char), text, args);
	va_end(args);
	drawText_(font, x, y, clr, buffer, 0, NULL);
}

void drawTextTruncate(u32 font, u32 x, u32 y, color_t clr, u32 max_width, const char* end_text, const char* text, ...) {
	char buffer[256];
	va_list args;
	va_start(args, text);
	vsnprintf(buffer, sizeof(buffer) / sizeof(char), text, args);
	va_end(args);
	drawText_(font, x, y, clr, text, max_width, end_text);
}

void getTextDimensions(u32 font, const char* text, u32* width_out, u32* height_out) {
	if (s_font_faces_total == 0) return;
	if (!FontSetType(font)) return;
	s_font_lastusedface = s_font_faces[0];
	u32 x = 0;
	u32 width = 0, height = s_font_lastusedface->size->metrics.height / 64 - font * 3;

	while (*text) {
		glyph_t glyph;
		u32 codepoint = DecodeUTF8(&text);

		if (codepoint == '\n') {
			x = 0;
			height += s_font_lastusedface->size->metrics.height / 64;
			height -= font * 3;
			continue;
		}

		if (!FontLoadGlyph(&glyph, font, codepoint)) {
			if (!FontLoadGlyph(&glyph, font, '?')) continue;
		}

		x += glyph.advance;

		if (x > width) width = x;
	}

	if (width_out) *width_out = width;
	if (height_out) *height_out = height;
}

bool fontInit(void) {
	FT_Error ret = 0;

	for (u32 i = 0; i < FONT_FACES_MAX; i++) {
		s_font_facesret[i] = 1;
	}

	ret = FT_Init_FreeType(&s_font_library);
	s_font_libret = ret;
	if (s_font_libret) return false;

	PlFontData fonts[PlSharedFontType_Total];
	Result rc = 0;
	rc = plGetSharedFont(s_textLanguageCode, fonts, FONT_FACES_MAX, &s_font_faces_total);
	if (R_FAILED(rc)) return false;

	for (u32 i = 0; i < s_font_faces_total; i++) {
		ret = FT_New_Memory_Face(s_font_library, (const FT_Byte*)fonts[i].address, fonts[i].size, 0, &s_font_faces[i]);

		s_font_facesret[i] = ret;
		if (ret) return false;
	}

	return true;
}

void fontExit() {
	for (u32 i = 0; i < s_font_faces_total; i++) {
		if (s_font_facesret[i] == 0) {
			FT_Done_Face(s_font_faces[i]);
		}
	}

	if (s_font_libret == 0) {
		FT_Done_FreeType(s_font_library);
	}
}

void drawRect(u32 x, u32 y, u32 w, u32 h, color_t color) {
	if (x + w > currentFBWidth) w = (x + w) - currentFBWidth;
	if (y + h > currentFBHeight) h = (y + h) - currentFBHeight;

	u32 mulR = (u32)color.r * (u32)color.a;
	u32 mulG = (u32)color.g * (u32)color.a;
	u32 mulB = (u32)color.b * (u32)color.a;
	u32 oneMinusAlpha = 255 - color.a;

	uint16x8_t alpha = vdupq_n_u16((u16)color.a);

	uint16x8_t vR = vdupq_n_u16((u16)mulR);
	uint16x8_t vG = vdupq_n_u16((u16)mulG);
	uint16x8_t vB = vdupq_n_u16((u16)mulB);

	uint16x8_t vOneMinusA = vdupq_n_u16(oneMinusAlpha);
	uint8x8_t fullAlpha = vdup_n_u8(255);

	u8* dst = currentFB + ((x + y * currentFBWidth) * 4);
	for (u32 j = 0; j < h; j++) {
		for (u32 i = 0; i < w / 8; i++) {
			uint8x8x4_t screenPixels = vld4_u8(&dst[i * 8 * 4]);
			screenPixels.val[0] =
			    vmovn_u16(vshrq_n_u16(vaddq_u16(vmulq_u16(vmovl_u8(screenPixels.val[0]), vOneMinusA), vR), 8));
			screenPixels.val[1] =
			    vmovn_u16(vshrq_n_u16(vaddq_u16(vmulq_u16(vmovl_u8(screenPixels.val[1]), vOneMinusA), vG), 8));
			screenPixels.val[2] =
			    vmovn_u16(vshrq_n_u16(vaddq_u16(vmulq_u16(vmovl_u8(screenPixels.val[2]), vOneMinusA), vB), 8));
			screenPixels.val[3] = fullAlpha;

			vst4_u8(&dst[i * 8 * 4], screenPixels);
		}

		for (u32 i = (w / 8) * 8; i < w; i++) {
			int baseIdx = i * 4;
			dst[baseIdx + 0] = ((u32)dst[baseIdx + 0] * oneMinusAlpha + mulR) >> 8;
			dst[baseIdx + 1] = ((u32)dst[baseIdx + 1] * oneMinusAlpha + mulG) >> 8;
			dst[baseIdx + 2] = ((u32)dst[baseIdx + 2] * oneMinusAlpha + mulB) >> 8;
			dst[baseIdx + 3] = 255;
		}

		dst += currentFBWidth * 4;
	}
}

void drawRectRaw(u32 x, u32 y, u32 w, u32 h, color_t color) {
	if (x + w > currentFBWidth) w = (x + w) - currentFBWidth;
	if (y + h > currentFBHeight) h = (y + h) - currentFBHeight;

	u32 color32 = color.r | (color.g << 8) | (color.b << 16) | (0xff << 24);
	u128 val = color32 | ((u128)color32 << 32) | ((u128)color32 << 64) | ((u128)color32 << 96);

	u32* dst = (u32*)(currentFB + (y * currentFBWidth + x) * 4);
	for (u32 j = 0; j < h; j++) {
		for (u32 i = 0; i < w; i += 4) {
			*((u128*)&dst[i]) = val;
		}

		dst += currentFBWidth;
	}
}