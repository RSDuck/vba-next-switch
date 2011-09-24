#ifndef GLOBALS_H
#define GLOBALS_H

#define VERBOSE_SWI                  1
#define VERBOSE_UNALIGNED_MEMORY     2
#define VERBOSE_ILLEGAL_WRITE        4
#define VERBOSE_ILLEGAL_READ         8
#define VERBOSE_DMA0                16
#define VERBOSE_DMA1                32
#define VERBOSE_DMA2                64
#define VERBOSE_DMA3               128
#define VERBOSE_UNDEFINED          256
#define VERBOSE_AGBPRINT           512
#define VERBOSE_SOUNDOUTPUT       1024

extern reg_pair reg[45];
extern bool ioReadable[0x400];
extern int saveType;
#ifdef USE_FRAMESKIP
extern bool speedup;
#endif
extern int cpuSaveType;
extern bool cheatsEnabled;
extern bool mirroringEnable;
extern bool enableRtc;
extern bool skipSaveGameBattery; // skip battery data when reading save states
extern bool skipSaveGameCheats;  // skip cheat list data when reading save states

extern uint8_t *rom;
extern uint8_t *pix;
extern uint8_t *ioMem;

#endif // GLOBALS_H
