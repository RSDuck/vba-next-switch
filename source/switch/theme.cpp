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
			currentTheme.keyboardBackgroundColor = MakeColor(240, 240, 240, 255);
			currentTheme.keyboardKeyBackgroundColor = MakeColor(231, 231, 231, 255);
			currentTheme.keyboardSPKeyTextColor = MakeColor(155, 155, 155, 255);
			currentTheme.keyboardSPKeyBackgroundColor = MakeColor(218, 218, 218, 255);
			currentTheme.keyboardOKKeyTextColor = MakeColor(253, 253, 253, 255);
			currentTheme.keyboardOKKeyBackgroundColor = MakeColor(50, 80, 240, 255);
			currentTheme.keyboardHighlightColor = MakeColor(16, 178, 203, 255);
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
			currentTheme.keyboardBackgroundColor = MakeColor(70, 70, 70, 255);
			currentTheme.keyboardKeyBackgroundColor = MakeColor(79, 79, 79, 255);
			currentTheme.keyboardSPKeyTextColor = MakeColor(150, 150, 150, 255);
			currentTheme.keyboardSPKeyBackgroundColor = MakeColor(96, 96, 96, 255);
			currentTheme.keyboardOKKeyTextColor = MakeColor(33, 77, 90, 255);
			currentTheme.keyboardOKKeyBackgroundColor = MakeColor(0, 248, 211, 255);
			currentTheme.keyboardHighlightColor = MakeColor(134, 241, 247, 255);
			currentTheme.btnA = btnALight;
			currentTheme.btnB = btnBLight;
			currentTheme.btnX = btnXLight;
			currentTheme.btnY = btnYLight;
			currentTheme.splashImage = splashBlack;
			break;
	}
}

theme_t currentTheme;