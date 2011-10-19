// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005-2006 Forgotten and the VBA development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#include <xtl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cmath>
extern "C" {
#include "fakegl.h"
}   

#include <time.h>

#include "../AutoBuild.h"

#include <SDL.h>

#include "../common/Patch.h"
#include "../gba/GBA.h"
#include "../gba/agbprint.h"
#include "../gba/Flash.h"
#include "../gba/Cheats.h"
#include "../gba/RTC.h"
#include "../gba/Sound.h"
#include "../gb/gb.h"
#include "../gb/gbGlobals.h"
#include "../gb/gbCheats.h"
#include "../gb/gbSound.h"
#include "../Util.h"

#include "debugger.h"
#include "filters.h"
#include "text.h"
#include "inputSDL.h"
#include "../common/SoundSDL.h"

#include "../Xbox/Main.h"

#ifndef _WIN32
# include <unistd.h>
# define GETCWD getcwd
#else // _WIN32
# include <direct.h>
# define GETCWD _getcwd
#endif // _WIN32

#ifndef __GNUC__
# define HAVE_DECL_GETOPT 0
# define __STDC__ 1
# include "getopt.h"
#else // ! __GNUC__
# define HAVE_DECL_GETOPT 1
# include <getopt.h>
#endif // ! __GNUC__


#define snprintf _snprintf;
extern void remoteInit();
extern void remoteCleanUp();
extern void remoteStubMain();
extern void remoteStubSignal(int,int);
extern void remoteOutput(const char *, u32);
extern void remoteSetProtocol(int);
extern void remoteSetPort(int);
extern CVBA360App app;
extern IDirect3DDevice9 *pDevice;

extern t_config config;

struct EmulatedSystem emulator = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	false,
	0
};

SDL_Surface *surface = NULL;

int systemSpeed = 0;
int systemRedShift = 0;
int systemBlueShift = 0;
int systemGreenShift = 0;
int systemColorDepth = 0;
int systemDebug = 0;
int systemVerbose = 0;
int systemFrameSkip = 0;
int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

int srcPitch = 0;
int srcWidth = 0;
int srcHeight = 0;
int destWidth = 0;
int destHeight = 0;
int desktopWidth = 0;
int desktopHeight = 0;

Filter filter = kStretch1x;
u8 *delta = NULL;

int filter_enlarge = 2;

int sdlPrintUsage = 0;

int cartridgeType = 3;
int captureFormat = 0;

int openGL = 0;
int textureSize = 256;
GLuint screenTexture = 0;
u8 *filterPix = 0;

int pauseWhenInactive = 0;
int active = 1;
int emulating = 0;
int RGB_LOW_BITS_MASK=0x821;
u32 systemColorMap32[0x10000];
//u16 systemColorMap16[0x10000];
u16 systemGbPalette[24];
FilterFunc filterFunction = 0;
IFBFilterFunc ifbFunction = 0;
IFBFilter ifbType = kIFBNone;
char filename[2048];
char biosFileName[2048];
char gbBiosFileName[2048];
char captureDir[2048];
char saveDir[2048];
char batteryDir[2048];
char* homeDir = NULL;

// Directory within homedir to use for default save location.
#define DOT_DIR ".vbam"

static char *rewindMemory = NULL;
static int *rewindSerials = NULL;
static int rewindPos = 0;
static int rewindSerial = 0;
static int rewindTopPos = 0;
static int rewindCounter = 0;
static int rewindCount = 0;
static bool rewindSaveNeeded = false;
static int rewindTimer = 0;

static int sdlSaveKeysSwitch = 0;
// if 0, then SHIFT+F# saves, F# loads (old VBA, ...)
// if 1, then SHIFT+F# loads, F# saves (linux snes9x, ...)
// if 2, then F5 decreases slot number, F6 increases, F7 saves, F8 loads

static int saveSlotPosition = 0; // default is the slot from normal F1
// internal slot number for undoing the last load
#define SLOT_POS_LOAD_BACKUP 8
// internal slot number for undoing the last save
#define SLOT_POS_SAVE_BACKUP 9

static int sdlOpenglScale = 1;
// will scale window on init by this much
static int sdlSoundToggledOff = 0;
// allow up to 100 IPS/UPS/PPF patches given on commandline
//#define PATCH_MAX_NUM 100
//int	sdl_patch_num	= 0;
//char *	(sdl_patch_names[PATCH_MAX_NUM])	= { NULL }; // and so on

extern int autoFireMaxCount;

#define REWIND_NUM 8
#define REWIND_SIZE 400000
#define SYSMSG_BUFFER_SIZE 1024

#define _stricmp strcasecmp

bool wasPaused = false;
int autoFrameSkip = 1;
int frameskipadjust = 0;
int showRenderedFrames = 0;
int renderedFrames = 0;

u32 throttleLastTime = 0;
u32 autoFrameSkipLastTime = 0;

int showSpeed = 1;
int showSpeedTransparent = 1;
bool disableStatusMessages = false;
bool paused = false;
bool pauseNextFrame = false;
bool debugger = false;
bool debuggerStub = false;
int fullscreen = 1;
int sdlFlashSize = 0;
int sdlAutoPatch = 1;
int sdlRtcEnable = 0;
int sdlAgbPrint = 0;
int sdlMirroringEnable = 0;

static int        ignore_first_resize_event = 0;

/* forward */
void systemConsoleMessage(const char*);

//void (*dbgMain)() = debuggerMain;
//void (*dbgSignal)(int,int) = debuggerSignal;
//void (*dbgOutput)(const char *, u32) = debuggerOutput;

int  mouseCounter = 0;

bool screenMessage = false;
char screenMessageBuffer[21];
u32  screenMessageTime = 0;

char *arg0;

int sdlPreparedCheats	= 0;
#define MAX_CHEATS 100
const char * sdlPreparedCheatCodes[MAX_CHEATS];

#define SDL_SOUND_MAX_VOLUME 2.0
#define SDL_SOUND_ECHO       0.2
#define SDL_SOUND_STEREO     0.15

#ifdef THREAD_FILTERS
struct PFTHREAD_DATA {
	void (*filterFunction)(u8*,u32,u8*,u8*,u32,int,int);
	u8 *sourcePointer;
	u32 sourcePitch;
	u8 *deltaPointer;
	u8* destPointer;
	u32 destPitch;
	int width;
	int height;
	int cpunum;
};

unsigned long WINAPI pfthread_func( LPVOID lpParameter)
{
	PFTHREAD_DATA *data = (PFTHREAD_DATA*)lpParameter;

	XSetThreadProcessor(GetCurrentThread(),data->cpunum);
	data->filterFunction(
			data->sourcePointer,
			data->sourcePitch,
			data->deltaPointer,
			data->destPointer,
			data->destPitch,
			data->width,
			data->height );

	return 0;
}

PFTHREAD_DATA         *pfthread_data;
HANDLE                *hThreads;
unsigned int           nThreads;
#endif

struct option sdlOptions[] = {
  { "agb-print", no_argument, &sdlAgbPrint, 1 },
  { "auto-frameskip", no_argument, &autoFrameSkip, 1 },
  { "bios", required_argument, 0, 'b' },
  { "config", required_argument, 0, 'c' },
  { "debug", no_argument, 0, 'd' },
  { "filter", required_argument, 0, 'f' },
  { "ifb-filter", required_argument, 0, 'I' },
  { "flash-size", required_argument, 0, 'S' },
  { "flash-64k", no_argument, &sdlFlashSize, 0 },
  { "flash-128k", no_argument, &sdlFlashSize, 1 },
  { "frameskip", required_argument, 0, 's' },
  { "fullscreen", no_argument, &fullscreen, 1 },
  { "gdb", required_argument, 0, 'G' },
  { "help", no_argument, &sdlPrintUsage, 1 },
  { "patch", required_argument, 0, 'i' },
  { "no-agb-print", no_argument, &sdlAgbPrint, 0 },
  { "no-auto-frameskip", no_argument, &autoFrameSkip, 0 },
  { "no-debug", no_argument, 0, 'N' },
  { "no-patch", no_argument, &sdlAutoPatch, 0 },
  { "no-opengl", no_argument, &openGL, 0 },
  { "no-pause-when-inactive", no_argument, &pauseWhenInactive, 0 },
  { "no-rtc", no_argument, &sdlRtcEnable, 0 },
  { "no-show-speed", no_argument, &showSpeed, 0 },
  { "opengl", required_argument, 0, 'O' },
  { "opengl-nearest", no_argument, &openGL, 1 },
  { "opengl-bilinear", no_argument, &openGL, 2 },
  { "pause-when-inactive", no_argument, &pauseWhenInactive, 1 },
  { "profile", optional_argument, 0, 'p' },
  { "rtc", no_argument, &sdlRtcEnable, 1 },
  { "save-type", required_argument, 0, 't' },
  { "save-auto", no_argument, &cpuSaveType, 0 },
  { "save-eeprom", no_argument, &cpuSaveType, 1 },
  { "save-sram", no_argument, &cpuSaveType, 2 },
  { "save-flash", no_argument, &cpuSaveType, 3 },
  { "save-sensor", no_argument, &cpuSaveType, 4 },
  { "save-none", no_argument, &cpuSaveType, 5 },
  { "show-speed-normal", no_argument, &showSpeed, 1 },
  { "show-speed-detailed", no_argument, &showSpeed, 2 },
  { "throttle", required_argument, 0, 'T' },
  { "verbose", required_argument, 0, 'v' },
  { "cheat", required_argument, 0, 1000 },
  { "autofire", required_argument, 0, 1001 },
  { NULL, no_argument, NULL, 0 }
};

static void sdlChangeVolume(float d)
{
	float oldVolume = soundGetVolume();
	float newVolume = oldVolume + d;

	if (newVolume < 0.0) newVolume = 0.0;
	if (newVolume > SDL_SOUND_MAX_VOLUME) newVolume = SDL_SOUND_MAX_VOLUME;

	if (fabs(newVolume - oldVolume) > 0.001) {
		char tmp[32];
		sprintf(tmp, "Volume: %i%%", (int)(newVolume*100.0+0.5));
		systemScreenMessage(tmp);
		soundSetVolume(newVolume);
	}
}

u32 sdlFromHex(char *s)
{
	u32 value;
	sscanf(s, "%x", &value);
	return value;
}

u32 sdlFromDec(char *s)
{
	u32 value = 0;
	sscanf(s, "%u", &value);
	return value;
}

#ifdef __MSC__
#define stat _stat
#define S_IFDIR _S_IFDIR
#endif

void sdlCheckDirectory(char *dir)
{
	struct stat buf;

	int len = strlen(dir);

	char *p = dir + len - 1;

	if(*p == '/' ||
			*p == '\\')
		*p = 0;

	if(stat(dir, &buf) == 0) {
		if(!(buf.st_mode & S_IFDIR)) {
			fprintf(stderr, "Error: %s is not a directory\n", dir);
			dir[0] = 0;
		}
	} else {
		fprintf(stderr, "Error: %s does not exist\n", dir);
		dir[0] = 0;
	}
}

char *sdlGetFilename(char *name)
{
	static char filebuffer[2048];

	int len = strlen(name);

	char *p = name + len - 1;

	while(true) {
		if(*p == '/' ||
				*p == '\\') {
			p++;
			break;
		}
		len--;
		p--;
		if(len == 0)
			break;
	}

	if(len == 0)
		strcpy(filebuffer, name);
	else
		strcpy(filebuffer, p);
	return filebuffer;
}

FILE *sdlFindFile(const char *name)
{
	char buffer[4096];
	char path[2048];

#ifdef _WIN32
#define PATH_SEP ";"
#define FILE_SEP '\\'
#define EXE_NAME "vbam.exe"
#else // ! _WIN32
#define PATH_SEP ":"
#define FILE_SEP '/'
#define EXE_NAME "vbam"
#endif // ! _WIN32

	fprintf(stdout, "Searching for file %s\n", name);

	FILE *f = fopen(name, "r");
	if(f != NULL)
		return f;
	else
		return NULL;
}

void sdlReadPreferences(FILE *f)
{
	char buffer[2048];

	do
	{
		char *s = fgets(buffer, 2048, f);

		if(s == NULL)
			break;

		char *p  = strchr(s, '#');

		if(p)
			*p = 0;

		char *token = strtok(s, " \t\n\r=");

		if(!token)
			continue;

		if(strlen(token) == 0)
			continue;

		char *key = token;
		char *value = strtok(NULL, "\t\n\r");

		if(value == NULL) {
			fprintf(stdout, "Empty value for key %s\n", key);
			continue;
		}

		if(!strcmp(key,"Joy0_Left")) {
			inputSetKeymap(PAD_1, KEY_LEFT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_Right")) {
			inputSetKeymap(PAD_1, KEY_RIGHT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_Up")) {
			inputSetKeymap(PAD_1, KEY_UP, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_Down")) {
			inputSetKeymap(PAD_1, KEY_DOWN, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_A")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_A, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_B")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_B, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_L")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_L, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_R")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_R, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_Start")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_START, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_Select")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_SELECT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_Speed")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_SPEED, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_Capture")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_CAPTURE, sdlFromHex(value));
		} else if(!strcmp(key,"Joy1_Left")) {
			inputSetKeymap(PAD_2, KEY_LEFT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_Right")) {
			inputSetKeymap(PAD_2, KEY_RIGHT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_Up")) {
			inputSetKeymap(PAD_2, KEY_UP, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_Down")) {
			inputSetKeymap(PAD_2, KEY_DOWN, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_A")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_A, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_B")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_B, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_L")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_L, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_R")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_R, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_Start")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_START, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_Select")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_SELECT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_Speed")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_SPEED, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_Capture")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_CAPTURE, sdlFromHex(value));
		} else if(!strcmp(key,"Joy2_Left")) {
			inputSetKeymap(PAD_3, KEY_LEFT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_Right")) {
			inputSetKeymap(PAD_3, KEY_RIGHT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_Up")) {
			inputSetKeymap(PAD_3, KEY_UP, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_Down")) {
			inputSetKeymap(PAD_3, KEY_DOWN, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_A")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_A, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_B")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_B, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_L")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_L, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_R")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_R, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_Start")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_START, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_Select")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_SELECT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_Speed")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_SPEED, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_Capture")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_CAPTURE, sdlFromHex(value));
		} else if(!strcmp(key,"Joy3_Left")) {
			inputSetKeymap(PAD_4, KEY_LEFT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_Right")) {
			inputSetKeymap(PAD_4, KEY_RIGHT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_Up")) {
			inputSetKeymap(PAD_4, KEY_UP, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_Down")) {
			inputSetKeymap(PAD_4, KEY_DOWN, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_A")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_A, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_B")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_B, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_L")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_L, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_R")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_R, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_Start")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_START, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_Select")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_SELECT, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_Speed")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_SPEED, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_Capture")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_CAPTURE, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_AutoA")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_AUTO_A, sdlFromHex(value));
		} else if(!strcmp(key, "Joy0_AutoB")) {
			inputSetKeymap(PAD_1, KEY_BUTTON_AUTO_B, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_AutoA")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_AUTO_A, sdlFromHex(value));
		} else if(!strcmp(key, "Joy1_AutoB")) {
			inputSetKeymap(PAD_2, KEY_BUTTON_AUTO_B, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_AutoA")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_AUTO_A, sdlFromHex(value));
		} else if(!strcmp(key, "Joy2_AutoB")) {
			inputSetKeymap(PAD_3, KEY_BUTTON_AUTO_B, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_AutoA")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_AUTO_A, sdlFromHex(value));
		} else if(!strcmp(key, "Joy3_AutoB")) {
			inputSetKeymap(PAD_4, KEY_BUTTON_AUTO_B, sdlFromHex(value));
		} else if(!strcmp(key, "openGL")) {
			openGL = sdlFromHex(value);
		} else if(!strcmp(key, "Motion_Left")) {
			inputSetMotionKeymap(KEY_LEFT, sdlFromHex(value));
		} else if(!strcmp(key, "Motion_Right")) {
			inputSetMotionKeymap(KEY_RIGHT, sdlFromHex(value));
		} else if(!strcmp(key, "Motion_Up")) {
			inputSetMotionKeymap(KEY_UP, sdlFromHex(value));
		} else if(!strcmp(key, "Motion_Down")) {
			inputSetMotionKeymap(KEY_DOWN, sdlFromHex(value));
		} else if(!strcmp(key, "frameSkip")) {
			frameSkip = sdlFromHex(value);
			if(frameSkip < 0 || frameSkip > 9)
				frameSkip = 2;
		} else if(!strcmp(key, "gbFrameSkip")) {
			gbFrameSkip = sdlFromHex(value);
			if(gbFrameSkip < 0 || gbFrameSkip > 9)
				gbFrameSkip = 0;
		} else if(!strcmp(key, "fullScreen")) {
			fullscreen = sdlFromHex(value) ? 1 : 0;
		} else if(!strcmp(key, "useBios")) {
			useBios = sdlFromHex(value) ? true : false;
		} else if(!strcmp(key, "skipBios")) {
			skipBios = sdlFromHex(value) ? true : false;
		} else if(!strcmp(key, "biosFile")) {
			strcpy(biosFileName, value);
		} else if(!strcmp(key, "gbBiosFile")) {
			strcpy(gbBiosFileName, value);
		} else if(!strcmp(key, "filter")) {
			filter = (Filter)sdlFromDec(value);
			if(filter < kStretch1x || filter >= kInvalidFilter)
				filter = kStretch2x;
		} else if(!strcmp(key, "disableStatus")) {
			disableStatusMessages = sdlFromHex(value) ? true : false;
		} else if(!strcmp(key, "borderOn")) {
			gbBorderOn = sdlFromHex(value) ? true : false;
		} else if(!strcmp(key, "borderAutomatic")) {
			gbBorderAutomatic = sdlFromHex(value) ? true : false;
		} else if(!strcmp(key, "emulatorType")) {
			gbEmulatorType = sdlFromHex(value);
			if(gbEmulatorType < 0 || gbEmulatorType > 5)
				gbEmulatorType = 1;
		} else if(!strcmp(key, "colorOption")) {
			gbColorOption = sdlFromHex(value) ? true : false;
		} else if(!strcmp(key, "captureDir")) {
			sdlCheckDirectory(value);
			strcpy(captureDir, value);
		} else if(!strcmp(key, "saveDir")) {
			sdlCheckDirectory(value);
			strcpy(saveDir, value);
		} else if(!strcmp(key, "batteryDir")) {
			sdlCheckDirectory(value);
			strcpy(batteryDir, value);
		} else if(!strcmp(key, "captureFormat")) {
			captureFormat = sdlFromHex(value);
		} else if(!strcmp(key, "soundQuality")) {
			int soundQuality = sdlFromHex(value);
			switch(soundQuality) {
				case 1:
				case 2:
				case 4:
					break;
				default:
					fprintf(stdout, "Unknown sound quality %d. Defaulting to 22Khz\n",
							soundQuality);
					soundQuality = 2;
					break;
			}
			soundSetSampleRate(44100 / soundQuality);
		} else if(!strcmp(key, "soundEnable")) {
			int res = sdlFromHex(value) & 0x30f;
			soundSetEnable(res);
		} else if(!strcmp(key, "soundStereo")) {
			if (sdlFromHex(value)) {
				gb_effects_config.stereo = SDL_SOUND_STEREO;
				gb_effects_config.enabled = true;
			}
		} else if(!strcmp(key, "soundEcho")) {
			if (sdlFromHex(value)) {
				gb_effects_config.echo = SDL_SOUND_ECHO;
				gb_effects_config.enabled = true;
			}
		} else if(!strcmp(key, "soundSurround")) {
			if (sdlFromHex(value)) {
				gb_effects_config.surround = true;
				gb_effects_config.enabled = true;
			}
		} else if(!strcmp(key, "declicking")) {
			gbSoundSetDeclicking(sdlFromHex(value) != 0);
		} else if(!strcmp(key, "soundVolume")) {
			float volume = sdlFromDec(value) / 100.0;
			if (volume < 0.0 || volume > SDL_SOUND_MAX_VOLUME)
				volume = 1.0;
			soundSetVolume(volume);
		} else if(!strcmp(key, "saveType")) {
			cpuSaveType = sdlFromHex(value);
			if(cpuSaveType < 0 || cpuSaveType > 5)
				cpuSaveType = 0;
		} else if(!strcmp(key, "flashSize")) {
			sdlFlashSize = sdlFromHex(value);
			if(sdlFlashSize != 0 && sdlFlashSize != 1)
				sdlFlashSize = 0;
		} else if(!strcmp(key, "ifbType")) {
			ifbType = (IFBFilter)sdlFromHex(value);
			if(ifbType < kIFBNone || ifbType >= kInvalidIFBFilter)
				ifbType = kIFBNone;
		} else if(!strcmp(key, "showSpeed")) {
			showSpeed = sdlFromHex(value);
			if(showSpeed < 0 || showSpeed > 2)
				showSpeed = 1;
		} else if(!strcmp(key, "showSpeedTransparent")) {
			showSpeedTransparent = sdlFromHex(value);
		} else if(!strcmp(key, "autoFrameSkip")) {
			autoFrameSkip = sdlFromHex(value);
		} else if(!strcmp(key, "pauseWhenInactive")) {
			pauseWhenInactive = sdlFromHex(value) ? true : false;
		} else if(!strcmp(key, "agbPrint")) {
			sdlAgbPrint = sdlFromHex(value);
		} else if(!strcmp(key, "rtcEnabled")) {
			sdlRtcEnable = sdlFromHex(value);
		} else if(!strcmp(key, "rewindTimer")) {
			rewindTimer = sdlFromHex(value);
			if(rewindTimer < 0 || rewindTimer > 600)
				rewindTimer = 0;
			rewindTimer *= 6;  // convert value to 10 frames multiple
		} else if(!strcmp(key, "saveKeysSwitch")) {
			sdlSaveKeysSwitch = sdlFromHex(value);
		} else if(!strcmp(key, "openGLscale")) {
			sdlOpenglScale = sdlFromHex(value);
		} else if(!strcmp(key, "autoFireMaxCount")) {
			autoFireMaxCount = sdlFromDec(value);
			if(autoFireMaxCount < 1)
				autoFireMaxCount = 1;
		}
		else
			fprintf(stderr, "Unknown configuration key %s\n", key);
	}while(1);
}

void sdlOpenGLInit(int w, int h)
{
	float screenAspect = (float) srcWidth / srcHeight,
	      windowAspect = (float) w / h;


	glDeleteTextures(1, &screenTexture);

	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);

	if(windowAspect == screenAspect)
		glViewport(0, 0, w, h);
	else if (windowAspect < screenAspect) {
		int height = (int)(w / screenAspect);
		glViewport(0, (h - height) / 2, w, height);
	} else {
		int width = (int)(h * screenAspect);
		glViewport((w - width) / 2, 0, width, h);
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(0.0, 1.0, 1.0, 0.0, 0.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glGenTextures(1, &screenTexture);
	glBindTexture(GL_TEXTURE_2D, screenTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
			openGL == 2 ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			openGL == 2 ? GL_LINEAR : GL_NEAREST);

	// Calculate texture size as a the smallest working power of two
	float n1 = log10((float)destWidth ) / log10( 2.0f);
	float n2 = log10((float)destHeight ) / log10( 2.0f);
	float n = (n1 > n2)? n1 : n2;

	// round up
	if (((float)((int)n)) != n)
		n = ((float)((int)n)) + 1.0f;

	textureSize = (int)pow(2.0f, n);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureSize, textureSize, 0,
			GL_BGRA, GL_UNSIGNED_BYTE, NULL);

	glClearColor(0.0,0.0,0.0,1.0);
	glClear( GL_COLOR_BUFFER_BIT );
}

void sdlReadPreferences()
{
	FILE *f = sdlFindFile("GAME:\\vbam.cfg");

  if(f == NULL) {
    fprintf(stdout, "Configuration file NOT FOUND (using defaults)\n");
    return;
  } else
    fprintf(stdout, "Reading configuration file.\n");

  sdlReadPreferences(f);

  fclose(f);
}

static void sdlApplyPerImagePreferences()
{
	FILE *f = sdlFindFile("GAME:\\vba-over.ini");
	if(!f)
	{
		fprintf(stdout, "vba-over.ini NOT FOUND (using emulator settings)\n");
		return;
	}
	else
		fprintf(stdout, "Reading vba-over.ini\n");

	char buffer[7];
	buffer[0] = '[';
	buffer[1] = rom[0xac];
	buffer[2] = rom[0xad];
	buffer[3] = rom[0xae];
	buffer[4] = rom[0xaf];
	buffer[5] = ']';
	buffer[6] = 0;

	char readBuffer[2048];

	bool found = false;

	do
	{
		char *s = fgets(readBuffer, 2048, f);

		if(s == NULL)
			break;

		char *p  = strchr(s, ';');

		if(p)
			*p = 0;

		char *token = strtok(s, " \t\n\r=");

		if(!token)
			continue;
		if(strlen(token) == 0)
			continue;

		if(!strcmp(token, buffer)) {
			found = true;
			break;
		}
	}while(1);

	if(found)
	{
		do
		{
			char *s = fgets(readBuffer, 2048, f);

			if(s == NULL)
				break;

			char *p = strchr(s, ';');
			if(p)
				*p = 0;

			char *token = strtok(s, " \t\n\r=");
			if(!token)
				continue;
			if(strlen(token) == 0)
				continue;

			if(token[0] == '[') // starting another image settings
				break;
			char *value = strtok(NULL, "\t\n\r=");
			if(value == NULL)
				continue;

			if(!strcmp(token, "rtcEnabled"))
				rtcEnable(atoi(value) == 0 ? false : true);
			else if(!strcmp(token, "flashSize")) {
				int size = atoi(value);
				if(size == 0x10000 || size == 0x20000)
					flashSetSize(size);
			} else if(!strcmp(token, "saveType")) {
				int save = atoi(value);
				if(save >= 0 && save <= 5)
					cpuSaveType = save;
			}
			else if(!strcmp(token, "mirroringEnabled"))
				mirroringEnable = (atoi(value) == 0 ? false : true);
		}while(1);
	}
	fclose(f);
}

static int sdlCalculateShift(u32 mask)
{
	int m = 0;

	while(mask)
	{
		m++;
		mask >>= 1;
	}

	return m-5;
}

/* returns filename of savestate num, in static buffer (not reentrant, no need to free,
 * but value won't survive much - so if you want to remember it, dup it)
 * You may use the buffer for something else though - until you call sdlStateName again
 */
static char * sdlStateName(int num)
{
	static char stateName[2048];

	if(saveDir[0])
		sprintf(stateName, "%s\\%s%d.sgm", saveDir, sdlGetFilename(filename), num+1);
	else if (homeDir)
		sprintf(stateName, "%s\\%s\\%s%d.sgm", homeDir, DOT_DIR, sdlGetFilename(filename), num + 1);
	else
		sprintf(stateName,"%s%d.sgm", filename, num+1);

	return stateName;
}

void sdlWriteState(int num)
{
	char * stateName;

	stateName = sdlStateName(num);

	if(emulator.emuWriteState)
		emulator.emuWriteState(stateName);

	// now we reuse the stateName buffer - 2048 bytes fit in a lot
	if (num == SLOT_POS_LOAD_BACKUP)
	{
		sprintf(stateName, "Current state backed up to %d", num+1);
		systemScreenMessage(stateName);
	}
	else if (num>=0)
	{
		sprintf(stateName, "Wrote state %d", num+1);
		systemScreenMessage(stateName);
	}

	systemDrawScreen();
}

void sdlReadState(int num)
{
	char * stateName;

	stateName = sdlStateName(num);
	if(emulator.emuReadState)
		emulator.emuReadState(stateName);

	if (num == SLOT_POS_LOAD_BACKUP)
		sprintf(stateName, "Last load UNDONE");
	else
		if (num == SLOT_POS_SAVE_BACKUP)
		{
			sprintf(stateName, "Last save UNDONE");
		}
		else
		{
			sprintf(stateName, "Loaded state %d", num+1);
		}
	systemScreenMessage(stateName);

	systemDrawScreen();
}

/*
 * perform savestate exchange
 * - put the savestate in slot "to" to slot "backup" (unless backup == to)
 * - put the savestate in slot "from" to slot "to" (unless from == to)
 */
void sdlWriteBackupStateExchange(int from, int to, int backup)
{
	char * dmp;
	char * stateNameOrig	= NULL;
	char * stateNameDest	= NULL;
	char * stateNameBack	= NULL;

	dmp		= sdlStateName(from);
	stateNameOrig = (char*)realloc(stateNameOrig, strlen(dmp) + 1);
	strcpy(stateNameOrig, dmp);
	dmp		= sdlStateName(to);
	stateNameDest = (char*)realloc(stateNameDest, strlen(dmp) + 1);
	strcpy(stateNameDest, dmp);
	dmp		= sdlStateName(backup);
	stateNameBack = (char*)realloc(stateNameBack, strlen(dmp) + 1);
	strcpy(stateNameBack, dmp);

	/* on POSIX, rename would not do anything anyway for identical names, but let's check it ourselves anyway */
	if (to != backup) {
		if (-1 == rename(stateNameDest, stateNameBack)) {
			fprintf(stdout, "savestate backup: can't backup old state %s to %s", stateNameDest, stateNameBack );
			perror(": ");
		}
	}
	if (to != from) {
		if (-1 == rename(stateNameOrig, stateNameDest)) {
			fprintf(stdout, "savestate backup: can't move new state %s to %s", stateNameOrig, stateNameDest );
			perror(": ");
		}
	}

	systemConsoleMessage("Savestate store and backup committed"); // with timestamp and newline
	fprintf(stdout, "to slot %d, backup in %d, using temporary slot %d\n", to+1, backup+1, from+1);

	free(stateNameOrig);
	free(stateNameDest);
	free(stateNameBack);
}

void sdlWriteBattery()
{
	char buffer[1048];

#if defined (_XBOX)
	if(batteryDir[0])
		sprintf(buffer, "%s\\%s.sav", batteryDir, sdlGetFilename(filename));
	else if (homeDir)
		sprintf(buffer, "%s\\%s\\%s.sav", homeDir, DOT_DIR, sdlGetFilename(filename));
	else
		sprintf(buffer, "%s.sav", filename);
#else
	sprintf(buffer, "%s.sav", sdlGetFilename(filename));
#endif

	emulator.emuWriteBattery(buffer);

	systemScreenMessage("Wrote battery");
}

void sdlReadBattery()
{
	char buffer[1048];

#if defined (_XBOX)
	if(batteryDir[0])
		sprintf(buffer, "%s\\%s.sav", batteryDir, sdlGetFilename(filename));
	else if (homeDir)
		sprintf(buffer, "%s\\%s\\%s.sav", homeDir, DOT_DIR, sdlGetFilename(filename));
	else
		sprintf(buffer, "%s.sav", filename);
#else
	sprintf(buffer, "%s.sav", sdlGetFilename(filename));
#endif

	bool res = false;

	res = emulator.emuReadBattery(buffer);

	if(res)
		systemScreenMessage("Loaded battery");
}

void sdlReadDesktopVideoMode()
{
	const SDL_VideoInfo* vInfo = SDL_GetVideoInfo();
}

void sdlInitVideo()
{
	int flags;
	int screenWidth;
	int screenHeight;

	filter_enlarge = getFilterEnlargeFactor(filter);

	destWidth = filter_enlarge * srcWidth;
	destHeight = filter_enlarge * srcHeight;

	fullscreen = 1;

	flags = SDL_ANYFORMAT | (fullscreen ? SDL_FULLSCREEN : 0);
	if(openGL)
	{
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		flags |= SDL_OPENGL | SDL_RESIZABLE;
	}
	else
		flags |= SDL_HWSURFACE;

	if (fullscreen && openGL) {
		screenWidth = desktopWidth;
		screenHeight = desktopHeight;
	} else {
		screenWidth = destWidth;
		screenHeight = destHeight;
	}

	if (config.aspect)
		flags |=  SDL_RESIZABLE;		

	surface = SDL_SetVideoMode(screenWidth, screenHeight, 0, flags);

	if(surface == NULL) {
		systemMessage(0, "Failed to set video mode");
		SDL_Quit();
		exit(-1);
	}

	systemRedShift = sdlCalculateShift(surface->format->Rmask);
	systemGreenShift = sdlCalculateShift(surface->format->Gmask);
	systemBlueShift = sdlCalculateShift(surface->format->Bmask);

	systemColorDepth = surface->format->BitsPerPixel;

	if(systemColorDepth == 16)
		srcPitch = srcWidth*2 + 4;
	else
	{
		if(systemColorDepth == 32)
			srcPitch = srcWidth*4 + 4;
		else
			srcPitch = srcWidth*3;
	}

#ifdef THREAD_FILTERS

	nThreads = 4;

	// create pfthread_data
	pfthread_data = (PFTHREAD_DATA*)malloc( sizeof(PFTHREAD_DATA) * nThreads );


	// create thread handles
	hThreads = (HANDLE*)malloc( sizeof(HANDLE) * nThreads );


#endif

	if(openGL)
	{
		int scaledWidth = screenWidth * sdlOpenglScale;
		int scaledHeight = screenHeight * sdlOpenglScale;

		free(filterPix);
		filterPix = (u8 *)calloc(1, (systemColorDepth >> 3) * destWidth * destHeight);
		sdlOpenGLInit(screenWidth, screenHeight);

		if (	(!fullscreen)
				&&	sdlOpenglScale	> 1
				&&	scaledWidth	< desktopWidth
				&&	scaledHeight	< desktopHeight
		   ) {
			SDL_SetVideoMode(scaledWidth, scaledHeight, 0,
					SDL_OPENGL | SDL_RESIZABLE |
					(fullscreen ? SDL_FULLSCREEN : 0));
			sdlOpenGLInit(scaledWidth, scaledHeight);
			/* xKiv: it would seem that SDL_RESIZABLE causes the *previous* dimensions to be immediately
			 * reported back via the SDL_VIDEORESIZE event
			 */
			ignore_first_resize_event	= 1;
		}
	}

	if (config.pointfilter == 1)
	{
		pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);

	}
	else
	{
		pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	}

	if (config.showFPS == 1)
		showSpeed = 2;
	else
		showSpeed = 0;

	if (config.bilinear == 1)
		filter = kAdMame2x;
	else
		filter = kStretch2x;
}

#define MOD_KEYS    (KMOD_CTRL|KMOD_SHIFT|KMOD_ALT|KMOD_META)
#define MOD_NOCTRL  (KMOD_SHIFT|KMOD_ALT|KMOD_META)
#define MOD_NOALT   (KMOD_CTRL|KMOD_SHIFT|KMOD_META)
#define MOD_NOSHIFT (KMOD_CTRL|KMOD_ALT|KMOD_META)


/*
 * 04.02.2008 (xKiv): factored out from sdlPollEvents
 *
 */
void change_rewind(int howmuch)
{
	if(emulating && emulator.emuReadMemState && rewindMemory && rewindCount)
	{
		rewindPos = (rewindPos + rewindCount + howmuch) % rewindCount;
		emulator.emuReadMemState(
				&rewindMemory[REWIND_SIZE*rewindPos],
				REWIND_SIZE
				);
		rewindCounter = 0;
		{
			char rewindMsgBuffer[50];
			snprintf(rewindMsgBuffer, 50, "Rewind to %1d [%d]", rewindPos+1, rewindSerials[rewindPos]);
			rewindMsgBuffer[49]	= 0;
			systemConsoleMessage(rewindMsgBuffer);
		}
	}
}

/*
 * handle the F* keys (for savestates)
 * given the slot number and state of the SHIFT modifier, save or restore
 * (in savemode 3, saveslot is stored in saveSlotPosition and num means:
 *  4 .. F5: decrease slot number (down to 0)
 *  5 .. F6: increase slot number (up to 7, because 8 and 9 are reserved for backups)
 *  6 .. F7: save state
 *  7 .. F8: load state
 *  (these *should* be configurable)
 *  other keys are ignored
 * )
 */
static void sdlHandleSavestateKey(int num, int shifted)
{
	int action	= -1;
	// 0: load
	// 1: save
	int backuping	= 1; // controls whether we are doing savestate backups

	if ( sdlSaveKeysSwitch == 2 )
	{
		// ignore "shifted"
		switch (num)
		{
			// nb.: saveSlotPosition is base 0, but to the user, we show base 1 indexes (F## numbers)!
			case 4:
				if (saveSlotPosition > 0)
				{
					saveSlotPosition--;
					fprintf(stdout, "Changed savestate slot to %d.\n", saveSlotPosition + 1);
				} else
					fprintf(stderr, "Can't decrease slotnumber below 1.\n");
				return; // handled
			case 5:
				if (saveSlotPosition < 7)
				{
					saveSlotPosition++;
					fprintf(stdout, "Changed savestate slot to %d.\n", saveSlotPosition + 1);
				} else
					fprintf(stderr, "Can't increase slotnumber above 8.\n");
				return; // handled
			case 6:
				action	= 1; // save
				break;
			case 7:
				action	= 0; // load
				break;
			default:
				// explicitly ignore
				return; // handled
		}
	}

	if (sdlSaveKeysSwitch == 0 ) /* "classic" VBA: shifted is save */
	{
		if (shifted)
			action	= 1; // save
		else	action	= 0; // load
		saveSlotPosition	= num;
	}
	if (sdlSaveKeysSwitch == 1 ) /* "xKiv" VBA: shifted is load */
	{
		if (!shifted)
			action	= 1; // save
		else	action	= 0; // load
		saveSlotPosition	= num;
	}

	if (action < 0 || action > 1)
	{
		fprintf(
				stderr,
				"sdlHandleSavestateKey(%d,%d), mode %d: unexpected action %d.\n",
				num,
				shifted,
				sdlSaveKeysSwitch,
				action
		       );
	}

	if (action)
	{        /* save */
		if (backuping)
		{
			sdlWriteState(-1); // save to a special slot
			sdlWriteBackupStateExchange(-1, saveSlotPosition, SLOT_POS_SAVE_BACKUP); // F10
		} else {
			sdlWriteState(saveSlotPosition);
		}
	} else { /* load */
		if (backuping)
		{
			/* first back up where we are now */
			sdlWriteState(SLOT_POS_LOAD_BACKUP); // F9
		}
		sdlReadState(saveSlotPosition);
	}

} // sdlHandleSavestateKey

void sdlPollEvents()
{
	SDL_Event event;
	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
			case SDL_JOYHATMOTION:
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
			case SDL_JOYAXISMOTION:   
				inputProcessSDLEvent(event);
				break;  
			case SDL_QUIT:
				emulating = 0;
				break; 

				break;
		}
	}
}

void usage(char *cmd)
{
	printf("%s [option ...] file\n", cmd);
	printf("\
			\n\
			Options:\n\
			-O, --opengl=MODE            Set OpenGL texture filter\n\
			--no-opengl               0 - Disable OpenGL\n\
			--opengl-nearest          1 - No filtering\n\
			--opengl-bilinear         2 - Bilinear filtering\n\
			-F, --fullscreen             Full screen\n\
			-G, --gdb=PROTOCOL           GNU Remote Stub mode:\n\
			tcp      - use TCP at port 55555\n\
			tcp:PORT - use TCP at port PORT\n\
			pipe     - use pipe transport\n\
			-I, --ifb-filter=FILTER      Select interframe blending filter:\n\
			");
	for (int i  = 0; i < (int)kInvalidIFBFilter; i++)
		printf("                                %d - %s\n", i, getIFBFilterName((IFBFilter)i));
	printf("\
			-N, --no-debug               Don't parse debug information\n\
			-S, --flash-size=SIZE        Set the Flash size\n\
			--flash-64k               0 -  64K Flash\n\
			--flash-128k              1 - 128K Flash\n\
			-T, --throttle=THROTTLE      Set the desired throttle (5...1000)\n\
			-b, --bios=BIOS              Use given bios file\n\
			-c, --config=FILE            Read the given configuration file\n\
			-d, --debug                  Enter debugger\n\
			-f, --filter=FILTER          Select filter:\n\
			");
	for (int i  = 0; i < (int)kInvalidFilter; i++)
		printf("                                %d - %s\n", i, getFilterName((Filter)i));
	printf("\
			-h, --help                   Print this help\n\
			-i, --patch=PATCH            Apply given patch\n\
			-p, --profile=[HERTZ]        Enable profiling\n\
			-s, --frameskip=FRAMESKIP    Set frame skip (0...9)\n\
			-t, --save-type=TYPE         Set the available save type\n\
			--save-auto               0 - Automatic (EEPROM, SRAM, FLASH)\n\
			--save-eeprom             1 - EEPROM\n\
			--save-sram               2 - SRAM\n\
			--save-flash              3 - FLASH\n\
			--save-sensor             4 - EEPROM+Sensor\n\
			--save-none               5 - NONE\n\
			-v, --verbose=VERBOSE        Set verbose logging (trace.log)\n\
			1 - SWI\n\
			2 - Unaligned memory access\n\
			4 - Illegal memory write\n\
			8 - Illegal memory read\n\
			16 - DMA 0\n\
			32 - DMA 1\n\
			64 - DMA 2\n\
			128 - DMA 3\n\
			256 - Undefined instruction\n\
			512 - AGBPrint messages\n\
			\n\
			Long options only:\n\
			--agb-print              Enable AGBPrint support\n\
			--auto-frameskip         Enable auto frameskipping\n\
			--no-agb-print           Disable AGBPrint support\n\
			--no-auto-frameskip      Disable auto frameskipping\n\
			--no-patch               Do not automatically apply patch\n\
			--no-pause-when-inactive Don't pause when inactive\n\
			--no-rtc                 Disable RTC support\n\
			--no-show-speed          Don't show emulation speed\n\
			--no-throttle            Disable throttle\n\
			--pause-when-inactive    Pause when inactive\n\
			--rtc                    Enable RTC support\n\
			--show-speed-normal      Show emulation speed\n\
			--show-speed-detailed    Show detailed speed data\n\
			--cheat 'CHEAT'          Add a cheat\n\
			");
}

/*
 * 04.02.2008 (xKiv) factored out, reformatted, more usefuler rewinds browsing scheme
 */
void handleRewinds()
{
	int curSavePos; // where we are saving today [1]

	rewindCount++;  // how many rewinds will be stored after this store
	if(rewindCount > REWIND_NUM)
		rewindCount = REWIND_NUM;

	curSavePos	= (rewindTopPos + 1) % rewindCount; // [1] depends on previous

	if(
			emulator.emuWriteMemState
			&&
			emulator.emuWriteMemState(
				&rewindMemory[curSavePos*REWIND_SIZE],
				REWIND_SIZE
				)
	  ) {
		char rewMsgBuf[100];
		snprintf(rewMsgBuf, 100, "Remembered rewind %1d (of %1d), serial %d.", curSavePos+1, rewindCount, rewindSerial);
		rewMsgBuf[99]	= 0;
		systemConsoleMessage(rewMsgBuf);
		rewindSerials[curSavePos]	= rewindSerial;

		// set up next rewind save
		// - don't clobber the current rewind position, unless it is the original top
		if (rewindPos == rewindTopPos) {
			rewindPos = curSavePos;
		}
		// - new identification and top
		rewindSerial++;
		rewindTopPos = curSavePos;
		// for the rest of the code, rewindTopPos will be where the newest rewind got stored
	}
}

int gba_main(char *path, char *fname)
{
	char FullPath[_MAX_PATH];

	strcpy(filename,fname);
	strcpy(FullPath, path);
	strcat(FullPath, fname);

	fprintf(stdout, "VBA-M version %s [SDL]\n", VERSION);

	captureDir[0] = 0;
	saveDir[0] = 0;
	batteryDir[0] = 0;

	int op = -1;

	frameSkip = 2;
	gbBorderOn = 0;

	parseDebug = true;

	gb_effects_config.stereo = 0.0;
	gb_effects_config.echo = 0.0;
	gb_effects_config.surround = false;
	gb_effects_config.enabled = false;

	char buf[1024];
	struct stat s;

#ifndef _WIN32
	// Get home dir
	homeDir = getenv("HOME");
	snprintf(buf, 1024, "%s/%s", homeDir, DOT_DIR);
	// Make dot dir if not existent
	if (stat(buf, &s) == -1 || !S_ISDIR(s.st_mode))
		mkdir(buf, 0755);
#else
	homeDir = 0;
#endif

	sdlReadPreferences();

	if(rewindTimer) {
		rewindMemory = (char *)malloc(REWIND_NUM*REWIND_SIZE);
		rewindSerials = (int *)calloc(REWIND_NUM, sizeof(int)); // init to zeroes
	}

	if(sdlFlashSize == 0)
		flashSetSize(0x10000);
	else
		flashSetSize(0x20000);

	rtcEnable(sdlRtcEnable ? true : false);
	agbPrintEnable(sdlAgbPrint ? true : false);



	for(int i = 0; i < 24;) {
		systemGbPalette[i++] = (0x1f) | (0x1f << 5) | (0x1f << 10);
		systemGbPalette[i++] = (0x15) | (0x15 << 5) | (0x15 << 10);
		systemGbPalette[i++] = (0x0c) | (0x0c << 5) | (0x0c << 10);
		systemGbPalette[i++] = 0;
	}

	systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

	if(filename) {
		char *szFile = FullPath;
		u32 len = strlen(szFile);


		utilStripDoubleExtension(szFile, filename);
		char *p = strrchr(filename, '.');

		if(p)
			*p = 0;

		/*    if (sdlAutoPatch && sdl_patch_num == 0)
		      {
		      char * tmp;
		// no patch given yet - look for ROMBASENAME.ips
		tmp = (char *)malloc(strlen(filename) + 4 + 1);
		sprintf(tmp, "%s.ips", filename);
		sdl_patch_names[sdl_patch_num] = tmp;
		sdl_patch_num++;

		// no patch given yet - look for ROMBASENAME.ups
		tmp = (char *)malloc(strlen(filename) + 4 + 1);
		sprintf(tmp, "%s.ups", filename);
		sdl_patch_names[sdl_patch_num] = tmp;
		sdl_patch_num++;

		// no patch given yet - look for ROMBASENAME.ppf
		tmp = (char *)malloc(strlen(filename) + 4 + 1);
		sprintf(tmp, "%s.ppf", filename);
		sdl_patch_names[sdl_patch_num] = tmp;
		sdl_patch_num++;
		} */

		soundInit();

		bool failed = false;

		IMAGE_TYPE type = utilFindType(szFile);

		if(type == IMAGE_UNKNOWN) {
			systemMessage(0, "Unknown file type %s", szFile);
			exit(-1);
		}
		cartridgeType = (int)type;

		if(type == IMAGE_GB) {
			failed = !gbLoadRom(szFile);
			if(!failed) {
				gbGetHardwareType();

				// used for the handling of the gb Boot Rom
				if (gbHardware & 5)
					gbCPUInit(gbBiosFileName, useBios);

				cartridgeType = IMAGE_GB;
				emulator = GBSystem;
				int size = gbRomSize, patchnum;
				//for (patchnum = 0; patchnum < sdl_patch_num; patchnum++) {
				//  fprintf(stdout, "Trying patch %s%s\n", sdl_patch_names[patchnum],
				//    applyPatch(sdl_patch_names[patchnum], &gbRom, &size) ? " [success]" : "");
				//}
				if(size != gbRomSize) {
					extern bool gbUpdateSizes();
					gbUpdateSizes();
					gbReset();
				}
				gbReset();
			}
		} else if(type == IMAGE_GBA) {
			int size = CPULoadRom(szFile);
			failed = (size == 0);
			if(!failed) {
				sdlApplyPerImagePreferences();

				doMirroring(mirroringEnable);

				cartridgeType = 0;
				emulator = GBASystem;

				CPUInit(biosFileName, useBios);
				//int patchnum;
				//for (patchnum = 0; patchnum < sdl_patch_num; patchnum++) {
				//  fprintf(stdout, "Trying patch %s%s\n", sdl_patch_names[patchnum],
				//    applyPatch(sdl_patch_names[patchnum], &rom, &size) ? " [success]" : "");
				// }
				CPUReset();
			}
		}

		if(failed) {
			systemMessage(0, "Failed to load file %s", szFile);
			exit(-1);
		}
	} else {
		soundInit();
		cartridgeType = 0;
		strcpy(filename, "gnu_stub");
		rom = (u8 *)malloc(0x2000000);
		workRAM = (u8 *)calloc(1, 0x40000);
		bios = (u8 *)calloc(1,0x4000);
		internalRAM = (u8 *)calloc(1,0x8000);
		paletteRAM = (u8 *)calloc(1,0x400);
		vram = (u8 *)calloc(1, 0x20000);
		oam = (u8 *)calloc(1, 0x400);
		pix = (u8 *)calloc(1, 4 * 241 * 162);
		ioMem = (u8 *)calloc(1, 0x400);

		emulator = GBASystem;

		CPUInit(biosFileName, useBios);
		CPUReset();
	}

	sdlReadBattery();

	if(debuggerStub)
		remoteInit();

	int flags = SDL_INIT_VIDEO|SDL_INIT_AUDIO|
		SDL_INIT_TIMER|SDL_INIT_NOPARACHUTE;

	if(SDL_Init(flags)) {
		systemMessage(0, "Failed to init SDL: %s", SDL_GetError());
		exit(-1);
	}

	if(SDL_InitSubSystem(SDL_INIT_JOYSTICK)) {
		systemMessage(0, "Failed to init joystick support: %s", SDL_GetError());
	}

	inputInitJoysticks();

	if(cartridgeType == 0) {
		srcWidth = 240;
		srcHeight = 160;
		systemFrameSkip = frameSkip;
	} else if (cartridgeType == 1) {
		if(gbBorderOn) {
			srcWidth = 256;
			srcHeight = 224;
			gbBorderLineSkip = 256;
			gbBorderColumnSkip = 48;
			gbBorderRowSkip = 40;
		} else {
			srcWidth = 160;
			srcHeight = 144;
			gbBorderLineSkip = 160;
			gbBorderColumnSkip = 0;
			gbBorderRowSkip = 0;
		}
		systemFrameSkip = gbFrameSkip;
	} else {
		srcWidth = 320;
		srcHeight = 240;
	}

	sdlReadDesktopVideoMode();

	sdlInitVideo();

	filterFunction = initFilter(filter, systemColorDepth, srcWidth);
	if (!filterFunction) {
		fprintf(stderr,"Unable to init filter '%s'\n", getFilterName(filter));
		exit(-1);
	}

	if(systemColorDepth == 15)
		systemColorDepth = 16;

	if(systemColorDepth != 16 && systemColorDepth != 24 &&
			systemColorDepth != 32) {
		fprintf(stderr,"Unsupported color depth '%d'.\nOnly 16, 24 and 32 bit color depths are supported\n", systemColorDepth);
		exit(-1);
	}

	fprintf(stdout,"Color depth: %d\n", systemColorDepth);

	utilUpdateSystemColorMaps();

	if(delta == NULL) {
		delta = (u8*)malloc(322*242*4);
		XMemSet(delta, 255, 322*242*4);
	}

	ifbFunction = initIFBFilter(ifbType, systemColorDepth);

	emulating = 1;
	renderedFrames = 0;

	autoFrameSkipLastTime = throttleLastTime = systemGetClock();

	SDL_WM_SetCaption("VBA-M", NULL);

	// now we can enable cheats?
	{
		int i;
		for (i=0; i<sdlPreparedCheats; i++) {
			const char *	p;
			int	l;
			p	= sdlPreparedCheatCodes[i];
			l	= strlen(p);
			if (l == 17 && p[8] == ':') {
				fprintf(stdout,"Adding cheat code %s\n", p);
				cheatsAddCheatCode(p, p);
			} else if (l == 13 && p[8] == ' ') {
				fprintf(stdout,"Adding CBA cheat code %s\n", p);
				cheatsAddCBACode(p, p);
			} else if (l == 8) {
				fprintf(stdout,"Adding GB(GS) cheat code %s\n", p);
				gbAddGsCheat(p, p);
			} else {
				fprintf(stderr,"Unknown format for cheat code %s\n", p);
			}
		}
	}

	while(emulating) {     
		emulator.emuMain(emulator.emuCount);
		sdlPollEvents(); 
	}

	emulating = 0;
	fprintf(stdout,"Shutting down\n");
	remoteCleanUp();
	soundShutdown();

	if(gbRom != NULL || rom != NULL) {
		sdlWriteBattery();
		emulator.emuCleanUp();
	}

	if(delta) {
		free(delta);
		delta = NULL;
	}

	if(filterPix) {
		free(filterPix);
		filterPix = NULL;
	}

	//  for (int i = 0; i < sdl_patch_num; i++) {
	//    if(sdl_patch_names[i])
	//	{
	//		free(sdl_patch_names[i]);		
	//	}
	//}


	SDL_Quit();
	return 0;
}




#ifdef __WIN32__
extern "C" {
  int WinMain()
  {
	  return(main(__argc, __argv));
  }
}
#endif

void systemMessage(int num, const char *msg, ...)
{
	char buffer[SYSMSG_BUFFER_SIZE*2];
	va_list valist;

	va_start(valist, msg);
	vsprintf(buffer, msg, valist);

	fprintf(stderr, "%s\n", buffer);
	va_end(valist);
}

void drawScreenMessage(u8 *screen, int pitch, int x, int y, unsigned int duration)
{
	if(screenMessage) 
	{
		if(cartridgeType == 1 && gbBorderOn)
			gbSgbRenderBorder();

		screenMessage = false;
	}
}

void drawSpeed(u8 *screen, int pitch, int x, int y)
{
	char buffer[50];
	if(showSpeed == 1)
		sprintf(buffer, "%d%%", systemSpeed);
	else
		sprintf(buffer, "%3d%%(%d, %d fps)", systemSpeed,
				systemFrameSkip,
				showRenderedFrames);
}

void systemDrawScreen()
{
	unsigned int destPitch = surface->pitch;
	u8 *screen;

	renderedFrames++;

	screen = (u8*)surface->pixels;
	SDL_LockSurface(surface);

#ifdef THREAD_FILTERS

	u8 *start = pix + srcPitch;
	int src_height_per_thread = srcHeight / nThreads;
	int src_height_remaining = srcHeight - ( ( srcHeight / nThreads ) * nThreads );
	u32 src_bytes_per_thread = srcPitch * src_height_per_thread;

	int dst_height_per_thread = src_height_per_thread * 2;
	u32 dst_bytes_per_thread = surface->pitch * dst_height_per_thread;

	unsigned int i = nThreads - 1;

	// Use Multi Threading
	do {
		// create last thread first because it could have more work than the others (for eg. if nThreads = 3)
		// (last thread has to process the remaining lines if (height / nThreads) is not an integer)

		// configure thread
		pfthread_data[i].filterFunction = filterFunction;
		pfthread_data[i].sourcePointer = start + ( i * src_bytes_per_thread );
		pfthread_data[i].sourcePitch = srcPitch;
		pfthread_data[i].deltaPointer = (u8*)delta; // TODO: check if thread-safe
		pfthread_data[i].destPointer = ((u8*)screen) + ( i * dst_bytes_per_thread );
		pfthread_data[i].destPitch = destPitch;
		pfthread_data[i].width = srcWidth;
		pfthread_data[i].cpunum = i;

		if( i == ( nThreads - 1 ) ) {
			// last thread
			pfthread_data[i].height = src_height_per_thread + src_height_remaining;
		} else {
			// other thread
			pfthread_data[i].height = src_height_per_thread;
		}

		// create thread
		hThreads[i] = CreateThread(
				NULL,
				0,
				pfthread_func,
				&pfthread_data[i],
				0,
				NULL );



	}while(i--);

	// Wait until every thread has finished.
	WaitForMultipleObjects(
			nThreads,
			hThreads,
			TRUE,
			INFINITE );

	// Close all thread handles.
	for( i = 0 ; i < nThreads ; i++ )
		CloseHandle( hThreads[i] );

#else
	filterFunction(pix + srcPitch, srcPitch, delta, screen,
			destPitch, srcWidth, srcHeight);
#endif

	drawScreenMessage(screen, destPitch, 10, destHeight - 20, 3000);

	if (showSpeed && fullscreen)
		drawSpeed(screen, destPitch, 10, 20);

	SDL_UnlockSurface(surface);
	SDL_Flip(surface);

}

void systemSetTitle(const char *title)
{
	SDL_WM_SetCaption(title, NULL);
}

void systemShowSpeed(int speed)
{
	systemSpeed = speed;

	showRenderedFrames = renderedFrames;
	renderedFrames = 0;

	if(!fullscreen && showSpeed)
	{
		char buffer[80];
		if(showSpeed == 1)
			sprintf(buffer, "VBA-M - %d%%", systemSpeed);
		else
			sprintf(buffer, "VBA-M - %d%%(%d, %d fps)", systemSpeed,
					systemFrameSkip,
					showRenderedFrames);

		systemSetTitle(buffer);
	}
}

void systemFrame()
{
}

void system10Frames(int rate)
{
	u32 time = systemGetClock();
	if(!wasPaused && autoFrameSkip) {
		u32 diff = time - autoFrameSkipLastTime;
		int speed = 100;

		if(diff)
			speed = (1000000/rate)/diff;

		if(speed >= 98) {
			frameskipadjust++;

			if(frameskipadjust >= 3) {
				frameskipadjust=0;
				if(systemFrameSkip > 0)
					systemFrameSkip--;
			}
		} else {
			if(speed  < 80)
				frameskipadjust -= (90 - speed)/5;
			else if(systemFrameSkip < 9)
				frameskipadjust--;

			if(frameskipadjust <= -2) {
				frameskipadjust += 2;
				if(systemFrameSkip < 9)
					systemFrameSkip++;
			}
		}
	}
	if(rewindMemory) {
		if(++rewindCounter >= rewindTimer) {
			rewindSaveNeeded = true;
			rewindCounter = 0;
		}
	}

	if(systemSaveUpdateCounter) {
		if(--systemSaveUpdateCounter <= SYSTEM_SAVE_NOT_UPDATED) {
			sdlWriteBattery();
			systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
		}
	}

	wasPaused = false;
	autoFrameSkipLastTime = time;
}

void systemScreenCapture(int a)
{
	char buffer[2048];

	if(captureFormat) {
		if(captureDir[0])
			sprintf(buffer, "%s\\%s%02d.bmp", captureDir, sdlGetFilename(filename), a);
		else if (homeDir)
			sprintf(buffer, "%s\\%s\\%s%02d.bmp", homeDir, DOT_DIR, sdlGetFilename(filename), a);
		else
			sprintf(buffer, "%s%02d.bmp", filename, a);

		emulator.emuWriteBMP(buffer);
	} else {
		if(captureDir[0])
			sprintf(buffer, "%s\\%s%02d.png", captureDir, sdlGetFilename(filename), a);
		else if (homeDir)
			sprintf(buffer, "%s\\%s\\%s%02d.png", homeDir, DOT_DIR, sdlGetFilename(filename), a);
		else
			sprintf(buffer, "%s%02d.png", filename, a);
		emulator.emuWritePNG(buffer);
	}

	systemScreenMessage("Screen capture");
}

u32 systemGetClock()
{
	return SDL_GetTicks();
}

void systemGbPrint(u8 *data,int pages,int feed,int palette, int contrast)
{
}

/* xKiv: added timestamp */
void systemConsoleMessage(const char *msg)
{
	time_t now_time;
	struct tm now_time_broken;

	now_time		= time(NULL);
	now_time_broken	= *(localtime( &now_time ));
	fprintf(
			stdout,
			"%02d:%02d:%02d %02d.%02d.%4d: %s\n",
			now_time_broken.tm_hour,
			now_time_broken.tm_min,
			now_time_broken.tm_sec,
			now_time_broken.tm_mday,
			now_time_broken.tm_mon + 1,
			now_time_broken.tm_year + 1900,
			msg
	       );
}

void systemScreenMessage(const char *msg)
{
	screenMessage = true;
	screenMessageTime = systemGetClock();
	if(strlen(msg) > 20) {
		strncpy(screenMessageBuffer, msg, 20);
		screenMessageBuffer[20] = 0;
	} else
		strcpy(screenMessageBuffer, msg);

	systemConsoleMessage(msg);
}

bool systemCanChangeSoundQuality()
{
  return false;
}

bool systemPauseOnFrame()
{
	if(pauseNextFrame)
	{
		paused = true;
		pauseNextFrame = false;

		return true;
	}
	return false;
}

void systemGbBorderOn()
{
	srcWidth = 256;
	srcHeight = 224;
	gbBorderLineSkip = 256;
	gbBorderColumnSkip = 48;
	gbBorderRowSkip = 40;

	sdlInitVideo();

	filterFunction = initFilter(filter, systemColorDepth, srcWidth);
}

bool systemReadJoypads()
{
	return true;
}

u32 systemReadJoypad(int which)
{
	return inputReadJoypad(which);
}

void systemUpdateMotionSensor()
{
	inputUpdateMotionSensor();
}

int systemGetSensorX()
{
	return inputGetSensorX();
}

int systemGetSensorY()
{
	return inputGetSensorY();
}

SoundDriver * systemSoundInit()
{
	soundShutdown();

	return new SoundSDL();
}

void systemOnSoundShutdown()
{
}

void systemOnWriteDataToSoundBuffer(const u16 * finalWave, int length)
{
}
