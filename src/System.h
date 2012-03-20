#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

// For backwards compatibility with the original VBA defines

#ifdef __CELLOS_LV2__
extern uint32_t special_action_msg_expired;
extern char special_action_msg[256];
#endif

extern void systemDrawScreen (void);
extern bool systemReadJoypads (void);
extern uint32_t systemGetClock (void);
extern void systemMessage(int, const char *, ...);
#ifdef USE_MOTION_SENSOR
extern void systemUpdateMotionSensor (void);
extern int  systemGetSensorX (void);
extern int  systemGetSensorY (void);
#endif
extern bool systemCanChangeSoundQuality (void);
extern void systemShowSpeed(int);

// sound functions
extern void systemSoundPause (void);
extern void systemSoundResume (void);
extern void systemSoundReset (void);
extern void systemOnWriteDataToSoundBuffer(int16_t * finalWave, int length);

extern uint16_t systemColorMap16[0x10000];
extern uint32_t systemColorMap32[0x10000];

extern int systemRedShift;
extern int systemGreenShift;
extern int systemBlueShift;

extern int systemColorDepth;
extern int systemSaveUpdateCounter;
extern int systemSpeed;

#define SYSTEM_SAVE_UPDATED 30
#define SYSTEM_SAVE_NOT_UPDATED 0

#include "VbaNext.h"

#endif // SYSTEM_H
