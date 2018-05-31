#pragma once

#include <switch/types.h>

typedef union {
    u32 abgr;
    struct {
        u8 r,g,b,a;
    };
} color_t;

typedef struct {
    u8 width, height;
    int8_t posX, posY, advance, pitch;
    const u8* data;
} glyph_t;