/******************************************************************************* 
 * settings.h - VBANext PS3
 *
 *  Created on: August 20, 2011
********************************************************************************/

#ifndef __PS3SETTINGS_H_
#define __PS3SETTINGS_H_

#include "../emu-ps3-constants.h"

#define EMULATOR_SPECIFIC_SETTINGS_UINT32() \
	uint32_t	EmulatedSystem; \
	uint32_t	LastEmulatedSystem; \
	uint32_t	SGBBorders;

struct SSettings
{
	int		PS3OverscanAmount;
	uint32_t	PS3FontSize;
	uint32_t	ControlStyle;
	uint32_t	PS3KeepAspect;
	uint32_t	PS3Smooth;
	uint32_t	PS3Smooth2;
	uint32_t	PS3OverscanEnabled;
	uint32_t	PS3PALTemporalMode60Hz;
	uint32_t	PS3CurrentResolution;
	uint32_t	SoundMode;
	uint32_t	ControlScheme;
	uint32_t	SaveCustomControlScheme;
	uint32_t	TripleBuffering;
	uint32_t	Throttled;
	uint32_t	ScaleEnabled;
	uint32_t	ScaleFactor;
	uint32_t	ViewportX;
	uint32_t	ViewportY;
	uint32_t	ViewportWidth;
	uint32_t	ViewportHeight;
	uint32_t	CurrentSaveStateSlot;
	uint32_t	ApplyShaderPresetOnStartup;
	uint32_t	ScreenshotsEnabled;
	EMULATOR_SPECIFIC_SETTINGS_UINT32();
	char		PS3PathSaveStates[MAX_PATH_LENGTH];
	char		PS3PathSRAM[MAX_PATH_LENGTH];
	char		PS3PathROMDirectory[MAX_PATH_LENGTH];
	char		RSoundServerIPAddress[MAX_PATH_LENGTH];
	char		GBABIOS[MAX_PATH_LENGTH];
	char		PS3CurrentShader[MAX_PATH_LENGTH];
	char		PS3CurrentShader2[MAX_PATH_LENGTH];
	char		ShaderPresetPath[MAX_PATH_LENGTH];
	char		ShaderPresetTitle[MAX_PATH_LENGTH];
	char		PS3CurrentBorder[MAX_PATH_LENGTH];
	char		GameAwareShaderPath[MAX_PATH_LENGTH];
	char		PS3CurrentInputPresetTitle[MAX_PATH_LENGTH];
};

extern struct SSettings			Settings;

#endif
