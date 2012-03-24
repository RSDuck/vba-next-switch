#ifndef GLOBALS_H
#define GLOBALS_H

#include "types.h"

extern bool ioReadable[0x400];
extern int saveType;
extern bool useBios;
extern bool skipBios;
extern int frameSkip;
extern bool cpuIsMultiBoot;
extern int cpuSaveType;
extern bool mirroringEnable;
extern bool enableRtc;
extern bool skipSaveGameBattery; // skip battery data when reading save states
extern bool skipSaveGameCheats;  // skip cheat list data when reading save states

extern u8 *bios;
extern u8 *rom;
extern u8 *internalRAM;
extern u8 *workRAM;
extern u8 *vram;
extern u8 *pix;
extern u8 *oam;
extern u8 *ioMem;

#endif // GLOBALS_H
