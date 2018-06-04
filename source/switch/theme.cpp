#include "theme.h"

static Image btnADark, btnALight, btnBDark, btnBLight, btnXDark, btnXLight, btnYDark, btnYLight, splashWhite, splashBlack;

void themeInit() {
	imageLoad(&btnADark, "romfs:/btnADark.png");
	imageLoad(&btnALight, "romfs:/btnALight.png");
	imageLoad(&btnBDark, "romfs:/btnBDark.png");
	imageLoad(&btnBLight, "romfs:/btnBLight.png");
	imageLoad(&btnXDark, "romfs:/btnXDark.png");
	imageLoad(&btnXLight, "romfs:/btnXLight.png");
	imageLoad(&btnYDark, "romfs:/btnYDark.png");
	imageLoad(&btnYLight, "romfs:/btnYLight.png");

	imageLoad(&splashWhite, "romfs:/splashWhite.png");
	imageLoad(&splashBlack, "romfs:/splashBlack.png");
}

void themeDeinit() {
	imageDeinit(&splashWhite);
	imageDeinit(&splashBlack);

	imageDeinit(&btnADark);
	imageDeinit(&btnALight);
	imageDeinit(&btnBDark);
	imageDeinit(&btnBLight);
	imageDeinit(&btnXDark);
	imageDeinit(&btnXLight);
	imageDeinit(&btnYDark);
	imageDeinit(&btnYLight);
}

void themeSet(themeMode_t t) {
	switch (t) {
		case modeLight:
			currentTheme.backgroundColor = MakeColor(239, 239, 239, 255);
			currentTheme.textColor = MakeColor(45, 45, 45, 255);
			currentTheme.textActiveColor = MakeColor(50, 80, 240, 255);
			currentTheme.textActiveBGColor = MakeColor(253, 253, 253, 255);
			currentTheme.btnA = btnADark;
			currentTheme.btnB = btnBDark;
			currentTheme.btnX = btnXDark;
			currentTheme.btnY = btnYDark;
			currentTheme.splashImage = splashWhite;
			break;
		case modeDark:
			currentTheme.backgroundColor = MakeColor(45, 45, 45, 255);
			currentTheme.textColor = MakeColor(255, 255, 255, 255);
			currentTheme.textActiveColor = MakeColor(0, 255, 197, 255);
			currentTheme.textActiveBGColor = MakeColor(33, 34, 39, 255);
			currentTheme.btnA = btnALight;
			currentTheme.btnB = btnBLight;
			currentTheme.btnX = btnXLight;
			currentTheme.btnY = btnYLight;
			currentTheme.splashImage = splashBlack;
			break;
	}
}

theme_t currentTheme;