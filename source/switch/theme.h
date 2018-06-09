#pragma once

#include "colors.h"
#include "image.h"

typedef struct {
	color_t backgroundColor;
	color_t textColor;
	color_t textActiveColor;
	color_t textActiveBGColor;
	color_t keyboardBackgroundColor;
	color_t keyboardKeyBackgroundColor;
	color_t keyboardSPKeyTextColor;
	color_t keyboardSPKeyBackgroundColor;
	color_t keyboardOKKeyTextColor;
	color_t keyboardOKKeyBackgroundColor;
	color_t keyboardHighlightColor;
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