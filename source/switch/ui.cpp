#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <vector>
#include <math.h> 

#include <switch.h>

extern "C" {
#include "ini/ini.h"
#include "localtime.h"
}

#include "draw.h"
#include "image.h"

#include "../system.h"
#include "../types.h"
#include "ui.h"
#include "util.h"
#include "colors.h"

#define FILENAMEBUFFER_SIZE (1024 * 32)  // 32kb
#define FILENAMES_COUNT_MAX 2048

#define SETTINGS_MAX (128)

static char* filenameBuffer = NULL;
static char* filenames[FILENAMES_COUNT_MAX];
static int filenamesCount = 0;

static char statusMessage[FILENAMES_COUNT_MAX];
static int statusMessageFadeout = 0;

static char selectedPath[PATH_LENGTH] = {'\0'};
static char currentDirectory[PATH_LENGTH] = {'\0'};
static int cursor = 0;
static int scroll = 0;

static const char *pauseMenuItems[] = {"Continue", "Load Savestate", "Write Savestate", "Settings", "Exit"};

Setting* settings;
Setting* tempSettings;
static int settingsMetaStart = 0;
static int settingsCount = 0;
static char* settingStrings[SETTINGS_MAX];

#define UI_STATESTACK_MAX 4
static UIState uiStateStack[UI_STATESTACK_MAX];
static int uiStateStackCount = 0;

static int rowsVisible = 0;

static Image gbaImage, logoSmall;

Image btnADark, btnALight, btnBDark, btnBLight, btnXDark, btnXLight, btnYDark, btnYLight;

u32 btnMargin = 0;

static const char* settingsPath = "vba-switch.ini";

uint32_t themeM = 0;
uint32_t useSwitchTheme = 1;

ColorSetId switchColorSetID;

static void generateSettingString(Setting *setting) {
	if (!setting->meta) {
		snprintf(setting->generatedString, sizeof(setting->generatedString) - 1, "%s: %s", setting->name,
			 setting->strValues[*setting->valueIdx]);
	} else {
		strcpy_safe(setting->generatedString, setting->name, sizeof(setting->generatedString));
	}
}

void uiStatusMsg(const char* format, ...) {
	statusMessageFadeout = 500;
	va_list args;
	va_start(args, format);
	vsnprintf(statusMessage, sizeof(statusMessage) / sizeof(char), format, args);
	va_end(args);
}

static void enterDirectory() {
	filenamesCount = FILENAMES_COUNT_MAX;
	getDirectoryContents(filenameBuffer, &filenames[0], &filenamesCount, currentDirectory, "gba");

	cursor = 0;
	scroll = 0;
}

void uiInit() {
	setupLocalTimeOffset();

	filenameBuffer = (char*)malloc(FILENAMEBUFFER_SIZE);
	strcpy_safe(currentDirectory, "", PATH_LENGTH);
	enterDirectory();

	settings = (Setting*)malloc(SETTINGS_MAX * sizeof(Setting));

	imageLoad(&gbaImage, "romfs:/gba.png");
	imageLoad(&logoSmall, "romfs:/logoSmall.png");
	imageLoad(&btnADark, "romfs:/btnADark.png");
	imageLoad(&btnALight, "romfs:/btnALight.png");
	imageLoad(&btnBDark, "romfs:/btnBDark.png");
	imageLoad(&btnBLight, "romfs:/btnBLight.png");
	imageLoad(&btnXDark, "romfs:/btnXDark.png");
	imageLoad(&btnXLight, "romfs:/btnXLight.png");
	imageLoad(&btnYDark, "romfs:/btnYDark.png");
	imageLoad(&btnYLight, "romfs:/btnYLight.png");
}

void uiDeinit() {
	imageDeinit(&gbaImage);
	imageDeinit(&logoSmall);
	imageDeinit(&btnADark);
	imageDeinit(&btnALight);
	imageDeinit(&btnBDark);
	imageDeinit(&btnBLight);
	imageDeinit(&btnXDark);
	imageDeinit(&btnXLight);
	imageDeinit(&btnYDark);
	imageDeinit(&btnYLight);

	free(filenameBuffer);
	free(settings);
}

void uiFinaliseAndLoadSettings() {
	settingsMetaStart = settingsCount;

	// uiAddSetting("Remap Buttons", NULL, result)
	uiAddSetting("Save and return", NULL, resultSaveSettings, NULL, true);
	uiAddSetting("Cancel", NULL, resultCancelSettings, NULL, true);

	ini_t* cfg = ini_load(settingsPath);
	if (cfg) {
		for (int i = 0; i < settingsMetaStart; i++) {
			if (ini_sget(cfg, "misc", settings[i].name, "%d", settings[i].valueIdx)) generateSettingString(&settings[i]);
		}

		ini_free(cfg);
	}
}

void uiSaveSettings() {
	settings = tempSettings;
	
	FILE* f = fopen(settingsPath, "wt");
	if (f) {
		fprintf(f, "[Misc]\n");

		for (int i = 0; i < settingsMetaStart; i++) fprintf(f, "%s=%d\n", settings[i].name, *settings[i].valueIdx);

		fclose(f);
	}
}

void uiCancelSettings() {
	ini_t* cfg = ini_load(settingsPath);
	if (cfg) {
		for (int i = 0; i < settingsMetaStart; i++) {
			if (ini_sget(cfg, "misc", settings[i].name, "%d", settings[i].valueIdx)) generateSettingString(&settings[i]);
		}

		ini_free(cfg);
	}
}

void uiGetSelectedFile(char* out, int outLength) { strcpy_safe(out, selectedPath, outLength); }

static int lastDst = 80;

UIResult uiLoop(u32 keysDown) {
	UIState state = uiGetState();
	if (state == stateRemapButtons) {
		//imageDraw(fb, currentFBWidth, currentFBHeight, &gbaImage, 0, 0);
	} else if (uiGetState() != stateRunning) {	
		if (useSwitchTheme) setTheme((themeMode)switchColorSetID);
		else themeM ? setTheme(LIGHT) : setTheme(DARK); //TODO improve this...

		btnMargin = 0;

		int scrollAmount = 0;
		if (keysDown & KEY_DOWN) scrollAmount = 1;
		if (keysDown & KEY_UP) scrollAmount = -1;
		if (keysDown & KEY_LEFT) scrollAmount = -5;
		if (keysDown & KEY_RIGHT) scrollAmount = 5;

		const char** menu = NULL;
		int menuItemsCount;
		if (state == stateSettings) {
			menu = (const char**)settingStrings;
			menuItemsCount = settingsCount;
		} else if (state == statePaused) {
			menu = pauseMenuItems;
			menuItemsCount = sizeof(pauseMenuItems) / sizeof(pauseMenuItems[0]);
		} else {
			menu = (const char**)filenames;
			menuItemsCount = filenamesCount;
		}

		drawRect(0, 0, currentFBWidth, currentFBHeight, THEME.backgroundColor);

		imageDraw(&logoSmall, 52, 15, 0, 0, 0);

		int i = 0;
		int separator = 40;
		int menuMarginTop = 80;

		rowsVisible = (currentFBHeight - 70 - menuMarginTop) / separator;

		if (scrollAmount > 0) {
			for (int i = 0; i < scrollAmount; i++) {
				if (cursor < menuItemsCount - 1) {
					cursor++;
					if (cursor - scroll >= rowsVisible) {
						scroll++;
					}
				}
			}
		} else if (scrollAmount < 0) {
			for (int i = 0; i < -scrollAmount; i++) {
				if (cursor > 0) {
					cursor--;
					if (cursor - scroll < 0) {
						scroll--;
					}
				}
			}
		}

		for (int j = scroll; j < menuItemsCount; j++) {
			u32 h, w;
			getTextDimensions(font16, menu[j], &w, &h);
			u32 heightOffset = (40 - h) / 2;
			u32 textMarginTop = heightOffset + menuMarginTop;

			if (i * separator + textMarginTop > currentFBHeight - 85) break;

			if (i + scroll == cursor) {
				int dst = i * separator + menuMarginTop;
				int delta = (dst - lastDst) / 2.2;
				dst = floor(lastDst + delta);

				drawRect(0, dst, currentFBWidth / 1.25, separator, THEME.textActiveBGColor);
				if (lastDst == dst)
					drawText(font16, 60, i * separator + textMarginTop, THEME.textActiveColor, menu[j]);
				else
					drawText(font16, 60, i * separator + textMarginTop, THEME.textColor, menu[j]);
				lastDst = dst;
			} else {
				drawText(font16, 60, i * separator + textMarginTop, THEME.textColor, menu[j]);
			}

			i++;
			if (i >= rowsVisible) break;
		}

		u64 timestamp;
		//timeGetCurrentTime(TimeType_UserSystemClock, &timestamp);
		//time_t tim = (time_t)timestamp;
		struct tm* timeStruct = getRealLocalTime(); //localtime(&tim);

		char timeBuffer[64];
		snprintf(timeBuffer, 64, "%02i:%02i", timeStruct->tm_hour, timeStruct->tm_min);

		drawText(font24, currentFBWidth - 130, 45, THEME.textColor, timeBuffer);

		drawRect((u32)((currentFBWidth - 1220) / 2), currentFBHeight - 73, 1220, 1, THEME.textColor);

		//UI Drawing routines
		switch (state) {
			case stateFileselect:
				drawText(font16, 60, currentFBHeight - 43, THEME.textColor, currentDirectory);
				uiDrawTipButton(B, 1, "Back");
				uiDrawTipButton(A, 2, "Open");
				uiDrawTipButton(X, 3, "Exit VBA Next");
				break;
			case stateSettings:
				uiDrawTipButton(B, 1, "Cancel");
				uiDrawTipButton(A, 2, "OK");
				uiDrawTipButton(X, 3, "Exit VBA Next");
				break;
			case statePaused:
				uiDrawTipButton(B, 1, "Return Game");
				uiDrawTipButton(A, 2, "OK");
				uiDrawTipButton(X, 3, "Exit VBA Next");
				break;
			default:
				break;
		}

		if (keysDown & KEY_X) return resultExit;

		if (keysDown & KEY_A || keysDown & KEY_B) {
			if (state == stateFileselect) {
				if (keysDown & KEY_B) cursor = 0;

				char path[PATH_LENGTH] = {'\0'};

				if (!strcmp(filenames[cursor], "..")) {
					int length = strlen(currentDirectory);
					for (int i = length - 1; i >= 0; i--) {
						if (currentDirectory[i] == '/') {
							strncpy(path, currentDirectory, i);
							path[i] = '\0';
							break;
						}
					}
				} else
					snprintf(path, PATH_LENGTH, "%s/%s", currentDirectory, filenames[cursor]);

				if (isDirectory(path)) {
					strcpy_safe(currentDirectory, path, PATH_LENGTH);
					enterDirectory();
				} else {
					strcpy_safe(selectedPath, path, PATH_LENGTH);
					return resultSelectedFile;
				}
			} else if (state == stateSettings) {
				if (keysDown & KEY_B) return resultCancelSettings;

				Setting* setting = &tempSettings[cursor];

				if (setting->meta) return (UIResult)setting->valuesCount;
				*setting->valueIdx += (keysDown & KEY_A ? 1 : -1);
				if (*setting->valueIdx == UINT32_MAX) *setting->valueIdx = setting->valuesCount - 1;
				if (*setting->valueIdx >= setting->valuesCount) *setting->valueIdx = 0;

				generateSettingString(setting);

				return resultNone;
			} else {

				if (keysDown & KEY_B) return resultUnpause;

				switch (cursor) {
					case 0:
						return resultUnpause;
					case 1:
						return resultLoadState;
					case 2:
						return resultSaveState;
					case 3:
						tempSettings = settings;
						return resultOpenSettings;
					case 4:
						return resultClose;
				}
			}
		}
	}

	if (statusMessageFadeout > 0) {
		int fadeout = statusMessageFadeout > 255 ? 255 : statusMessageFadeout;
		drawText(font16, 60, currentFBHeight - 43, MakeColor(58, 225, 208, fadeout), statusMessage);
		statusMessageFadeout -= 10;
	}

	return resultNone;
}

void uiPushState(UIState state) {
	if (uiStateStackCount < UI_STATESTACK_MAX)
		uiStateStack[uiStateStackCount++] = state;
	else
		printf("warning: push UI stack further than max\n");

	cursor = 0;
	scroll = 0;
}

void uiPopState() {
	if (uiStateStackCount > 0)
		uiStateStackCount--;
	else
		printf("warning: pop empty UI stack\n");

	cursor = 0;
	scroll = 0;
}

UIState uiGetState() {
	if (uiStateStackCount == 0) {
		printf("warning: uiGetState() for empty UI stack");
		return stateFileselect;
	}
	return uiStateStack[uiStateStackCount - 1];
}

void uiAddSetting(const char* name, u32* valueIdx, u32 valuesCount, const char* strValues[], bool meta) {
	settings[settingsCount].name = name;
	settings[settingsCount].valueIdx = valueIdx;
	settings[settingsCount].valuesCount = valuesCount;
	settings[settingsCount].strValues = strValues;
	settings[settingsCount].meta = meta;

	settingStrings[settingsCount] = settings[settingsCount].generatedString;

	generateSettingString(&settings[settingsCount]);

	settingsCount++;
}

void uiDrawTipButton(buttonType type, u32 pos, const char* text) {
	u32 h, w;
	getTextDimensions(font16, text, &w, &h);
	h = (73 - h)  / 2;
	
	w += 25 + 13;
	pos == 1 ? btnMargin = w + 60 : btnMargin += 40 + w;
	u32 x = currentFBWidth - btnMargin;
	u32 y = currentFBHeight - 50;

	switch (type) {
		case A:
			imageDraw(&THEME.btnA, x, y, 0, 0, 0);
			drawText(font16, x + 25 + 13, currentFBHeight - 73 + h, THEME.textColor, text);
			break;
		case B:
			imageDraw(&THEME.btnB, x, y, 0, 0, 0);
			drawText(font16, x + 25 + 13, currentFBHeight - 73 + h, THEME.textColor, text);
			break;
		case Y:
			imageDraw(&THEME.btnY, x, y, 0, 0, 0);
			drawText(font16, x + 25 + 13, currentFBHeight - 73 + h, THEME.textColor, text);
			break;
		case X:
			imageDraw(&THEME.btnX, x, y, 0, 0, 0);
			drawText(font16, x + 25 + 13, currentFBHeight - 73 + h, THEME.textColor, text);
			break;
		default:
			break;
	}
}