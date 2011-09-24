#ifndef SYSTEM_H
#define SYSTEM_H

#ifdef __CELLOS_LV2__
#include "../../platform/common/utils/zlib/zlib.h"
#else
#include <zlib.h>
#endif

// For backwards compatibility with the original VBA defines
#ifdef FRAMESKIP
#define USE_FRAMESKIP
#endif

#ifdef __CELLOS_LV2__
#include "../../platform/ps3/src/ps3video.hpp"

extern PS3Graphics* Graphics;

extern uint32_t special_action_msg_expired;
extern char special_action_msg[256];
#endif

extern bool systemPauseOnFrame();
extern void systemGbPrint(uint8_t *,int,int,int,int);
extern void systemScreenCapture(int);
#ifndef __CELLOS_LV2__
extern void systemDrawScreen();
#endif

// updates the joystick data
extern bool systemReadJoypads();

// return information about the given joystick, -1 for default joystick
extern void systemReadJoypad(int);

extern uint32_t systemGetClock();
extern void systemMessage(int, const char *, ...);
extern void systemScreenMessage(const char *);
#ifdef USE_MOTION_SENSOR
extern void systemUpdateMotionSensor();
extern int  systemGetSensorX();
extern int  systemGetSensorY();
#endif
extern bool systemCanChangeSoundQuality();
extern void systemShowSpeed(int);
#ifdef USE_FRAMESKIP
extern void system10Frames(int);
extern void Sm60FPS_Init();
extern bool Sm60FPS_CanSkipFrame();
extern void Sm60FPS_Sleep();
#endif
extern void systemFrame();
extern void systemGbBorderOn();

// sound functions
extern bool systemSoundInit();
extern bool systemSoundInitDriver(long samplerate);
extern void systemSoundPause();
extern void systemSoundResume();
extern void systemSoundReset();
extern void systemSoundSetThrottle(unsigned short throttle);
extern void systemSoundNonblock(bool enable);
extern void systemOnWriteDataToSoundBuffer(int16_t * finalWave, int length);
extern void systemOnSoundShutdown();

//extern uint16_t systemColorMap16[0x10000];
extern uint32_t systemColorMap32[0x10000];
extern uint16_t systemGbPalette[24];

extern int systemRedShift;
extern int systemGreenShift;
extern int systemBlueShift;

#if defined(USE_GBA_FILTERS)
extern int systemColorDepth;
#endif

#ifdef USE_AGBPRINT
extern int systemVerbose;
#endif

extern int systemFrameSkip;
extern int systemSaveUpdateCounter;
extern int systemSpeed;

#define SYSTEM_SAVE_UPDATED 30
#define SYSTEM_SAVE_NOT_UPDATED 0

#include "VbaNext.h"

#endif // SYSTEM_H
