#pragma once

#include "colors.h"
#include "image.h"

typedef struct {
    color_t backgroundColor;
    color_t textColor;
    color_t textActiveColor;
    color_t textActiveBGColor;
    Image btnA;
    Image btnB;
    Image btnX;
    Image btnY;
} theme_t;

typedef enum {
    LIGHT,
    DARK,
} themeMode;

extern Image btnADark, btnALight, btnBDark, btnBLight, btnXDark, btnXLight, btnYDark, btnYLight;

void setTheme(themeMode t);

extern theme_t THEME;