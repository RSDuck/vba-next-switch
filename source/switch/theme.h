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
	Image splashImage;
} theme_t;

typedef enum {
	modeLight,
	modeDark,
} themeMode_t;

enum { switchTheme, darkTheme, lightTheme };

void themeInit();
void themeDeinit();

void themeSet(themeMode_t t);

extern theme_t currentTheme;