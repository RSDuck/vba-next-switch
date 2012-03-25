#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

extern void systemDrawScreen (void);
extern bool systemReadJoypads (void);
extern uint32_t systemGetClock (void);
extern void systemMessage(const char *, ...);
#ifdef USE_MOTION_SENSOR
extern void systemUpdateMotionSensor (void);
extern int  systemGetSensorX (void);
extern int  systemGetSensorY (void);
#endif

// sound functions
extern void systemOnWriteDataToSoundBuffer(int16_t * finalWave, int length);

extern uint16_t systemColorMap16[0x10000];
extern uint32_t systemColorMap32[0x10000];

extern int systemRedShift;
extern int systemGreenShift;
extern int systemBlueShift;

extern int systemColorDepth;

#endif // SYSTEM_H
