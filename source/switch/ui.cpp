#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <algorithm>
#include <vector>
#include <sstream>
#include <iostream>

#include <switch.h>

extern "C" {
#include "ini/ini.h"
#include "localtime.h"
}

#include "draw.h"
#include "image.h"
#include "hbkbd.h"

#include "../system.h"
#include "../types.h"
#include "../GBACheats.h"
#include "colors.h"
#include "ui.h"
#include "util.h"

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

static const char* pauseMenuItems[] = {"Continue", "Load Savestate", "Write Savestate", "Cheats", "Settings", "Exit"};

static const char* emptyCheatItems[] = {"Add a cheat with the plus-button"};

const char* themeOptions[] = {"Switch", "Dark", "Light"};

Setting* settings;
static int settingsMetaStart = 0;
static int settingsCount = 0;
static char* settingStrings[SETTINGS_MAX];

#define UI_STATESTACK_MAX 4
static UIState uiStateStack[UI_STATESTACK_MAX];
static int uiStateStackCount = 0;

static int rowsVisible = 0;

static Image gbaImage, logoSmall;

static u32 btnMargin = 0;

static const char* settingsPath = "vba-switch.ini";

const char* stringsNoYes[] = {"No", "Yes"};

static ColorSetId switchColorSetID;
static uint32_t themeM = 0;

static int lastDst = 80;
static int splashTime = 240;
static u32 splashEnabled = 1;

static void generateSettingString(Setting* setting) {
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

	themeInit();

	imageLoad(&gbaImage, "romfs:/gba.png");
	imageLoad(&logoSmall, "romfs:/logoSmall.png");

	setsysGetColorSetId(&switchColorSetID);

	uiAddSetting("Enable splash screen", &splashEnabled, 2, stringsNoYes);
	uiAddSetting("Theme", &themeM, 3, themeOptions);
	
	hbkbd::init();

	uiPushState(stateFileselect);
}

void uiDeinit() {
	themeDeinit();

	imageDeinit(&gbaImage);
	imageDeinit(&logoSmall);

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

void uiDraw(u32 keysDown) {
	UIState state = uiGetState();

	if (state == stateRunning) return;

	if (themeM == switchTheme)
		themeSet((themeMode_t)switchColorSetID);
	else if (themeM == lightTheme)
		themeSet(modeLight);
	else
		themeSet(modeDark);

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
	} else if (state == stateCheats) {
		menu = (const char**)cheatsStringList;
		menuItemsCount = cheatsNumber + 1;
	} else if (state == statePaused) {
		menu = pauseMenuItems;
		menuItemsCount = sizeof(pauseMenuItems) / sizeof(pauseMenuItems[0]);
	} else {
		menu = (const char**)filenames;
		menuItemsCount = filenamesCount;
	}

	drawRect(0, 0, currentFBWidth, currentFBHeight, currentTheme.backgroundColor);

	imageDraw(&logoSmall, 52, 15);

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
			float dst = i * separator + menuMarginTop;
			// float delta = (dst - lastDst) / 2.2;
			// dst = floor(lastDst + delta);

			drawRect(0, (u32)dst, currentFBWidth / 1.25, separator, currentTheme.textActiveBGColor);
			if (lastDst == dst)
				drawText(font16, 60, i * separator + textMarginTop, currentTheme.textActiveColor, menu[j]);
			else
				drawText(font16, 60, i * separator + textMarginTop, currentTheme.textColor, menu[j]);
			lastDst = dst;
		} else {
			drawText(font16, 60, i * separator + textMarginTop, currentTheme.textColor, menu[j]);
		}

		i++;
		if (i >= rowsVisible) break;
	}

	struct tm* timeStruct = getRealLocalTime();

	drawText(font24, currentFBWidth - 130, 45, currentTheme.textColor, "%02i:%02i", timeStruct->tm_hour, timeStruct->tm_min);

	drawRect((u32)((currentFBWidth - 1220) / 2), currentFBHeight - 73, 1220, 1, currentTheme.textColor);

	// UI Buttom Bar Buttons Drawing routines
	switch (state) {
		case stateFileselect:
			drawText(font16, 60, currentFBHeight - 43, currentTheme.textColor, currentDirectory);
			uiDrawTipButton(buttonB, 1, "Back");
			uiDrawTipButton(buttonA, 2, "Open");
			uiDrawTipButton(buttonX, 3, "Exit VBA Next");
			break;
		case stateCheats:
			uiDrawTipButton(buttonB, 1, "Back");
			uiDrawTipButton(buttonA, 2, "Toggle Cheat");
			uiDrawTipButton(buttonY, 3, "Delete Cheat");
			uiDrawTipButton(buttonX, 4, "Exit VBA Next");
			break;
		case stateSettings:
			uiDrawTipButton(buttonB, 1, "Cancel");
			uiDrawTipButton(buttonA, 2, "OK");
			uiDrawTipButton(buttonX, 3, "Exit VBA Next");
			break;
		case statePaused:
			uiDrawTipButton(buttonB, 1, "Return Game");
			uiDrawTipButton(buttonA, 2, "OK");
			uiDrawTipButton(buttonX, 3, "Exit VBA Next");
			break;
		default:
			break;
	}

	if (splashTime > 0) {
		if (splashEnabled) imageDraw(&currentTheme.splashImage, 0, 0, splashTime <= 120 ? splashTime * 255 / 120 : 255);
		splashTime -= 5;
	}
}

UIResult uiLoop(u32 keysDown) {
	UIState state = uiGetState();
	if (state == stateRemapButtons) {
		// imageDraw(fb, currentFBWidth, currentFBHeight, &gbaImage, 0, 0);
	} else if (uiGetState() != stateRunning) {
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

				Setting* setting = &settings[cursor];

				if (setting->meta) return (UIResult)setting->valuesCount;
				*setting->valueIdx += (keysDown & KEY_A ? 1 : -1);
				if (*setting->valueIdx == UINT32_MAX) *setting->valueIdx = setting->valuesCount - 1;
				if (*setting->valueIdx >= setting->valuesCount) *setting->valueIdx = 0;

				generateSettingString(setting);

				return resultNone;
			} else if (state == stateCheats) {
				if ((keysDown & KEY_B)) return resultCloseCheats;

				if (cursor < cheatsNumber) {
					if(cheatsList[cursor].enabled)
						cheatsDisable(cursor);
					else
						cheatsEnable(cursor);
				} else {
					std::istringstream iss(hbkbd::keyboard("Enter the gameshark cheat") );
					std::string cheatFirstPart;
					std::string cheatSecondPart;
					iss >> cheatFirstPart >> cheatSecondPart;
					if(cheatFirstPart.length() > 8 || cheatSecondPart.length() > 8) {
						uiStatusMsg("Invalid cheat-code. Are you sure this is a gameshark-code?");
						return resultNone;
					}
					cheatFirstPart = std::string( 8 - cheatFirstPart.length(), '0').append(cheatFirstPart);
        			cheatSecondPart = std::string( 8 - cheatSecondPart.length(), '0').append(cheatSecondPart);
					std::string fullCheat = cheatFirstPart.append(cheatSecondPart);
					std::transform(fullCheat.begin(), fullCheat.end(),fullCheat.begin(), ::toupper);

					std::cout << fullCheat << std::endl;
					cheatsAddGSACode(fullCheat.c_str(), hbkbd::keyboard("Enter a description for the cheat").c_str(), true);
				}
				
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
						return resultOpenCheats;
					case 4:
						return resultOpenSettings;
					case 5:
						return resultClose;
				}
			}
		} else if (keysDown & KEY_Y) {
			if (state == stateCheats && cursor < cheatsNumber) {
				cheatsDelete(cursor, true);
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
	h = (73 - h) / 2;

	w += 25 + 13;
	pos == 1 ? btnMargin = w + 60 : btnMargin += w + 40;
	u32 x = currentFBWidth - btnMargin;
	u32 y = currentFBHeight - 50;

	switch (type) {
		case buttonA:
			imageDraw(&currentTheme.btnA, x, y);
			drawText(font16, x + 25 + 13, currentFBHeight - 73 + h, currentTheme.textColor, text);
			break;
		case buttonB:
			imageDraw(&currentTheme.btnB, x, y);
			drawText(font16, x + 25 + 13, currentFBHeight - 73 + h, currentTheme.textColor, text);
			break;
		case buttonY:
			imageDraw(&currentTheme.btnY, x, y);
			drawText(font16, x + 25 + 13, currentFBHeight - 73 + h, currentTheme.textColor, text);
			break;
		case buttonX:
			imageDraw(&currentTheme.btnX, x, y);
			drawText(font16, x + 25 + 13, currentFBHeight - 73 + h, currentTheme.textColor, text);
			break;
		default:
			break;
	}
}