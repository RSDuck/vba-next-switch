#pragma once

#include "colors.h"

typedef struct {
    color_t backgroundColor;
    color_t textColor;
    color_t textActiveColor;
    color_t textActiveBGColor;
} theme_t;

typedef enum {
    LIGHT,
    DARK,
} themeMode;

void setTheme(themeMode t);

extern theme_t THEME;