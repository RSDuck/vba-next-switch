#ifndef __UI_H__
#define __UI_H__

#include "theme.h"

typedef enum {
	resultSelectedFile,
	resultClose,
	resultUnpause,
	resultExit,
	resultNone,
	resultLoadState,
	resultSaveState,
	resultSettingsChanged,
	resultShowCredits,
	resultOpenSettings,
	resultSaveSettings,
	resultCancelSettings
} UIResult;

typedef enum { stateRunning, stateFileselect, statePaused, stateSettings, stateRemapButtons } UIState;

struct Setting {
	const char* name;
	u32 valuesCount, *valueIdx;
	const char** strValues;
	char generatedString[256];
	bool meta;
};

extern Setting* settings;
extern Setting* tempSettings;

#define PATH_LENGTH 512

void uiInit();
void uiDeinit();

void uiGetSelectedFile(char* out, int outLength);

UIResult uiLoop(u32 keysDown);
void uiPushState(UIState state);
void uiPopState();
UIState uiGetState();

void uiAddSetting(const char* name, u32* valueIdx, u32 valuesCount, const char* strValues[], bool meta = false);
void uiFinaliseAndLoadSettings();
void uiSaveSettings();
void uiCancelSettings();

extern uint32_t themeM;
extern uint32_t useSwitchTheme;

extern ColorSetId switchColorSetID;

void uiStatusMsg(const char* fmt, ...);

#endif