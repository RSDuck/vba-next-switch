#ifndef GBA_H
#define GBA_H

#include "../System.h"

#define SAVE_GAME_VERSION_1 1
#define SAVE_GAME_VERSION_2 2
#define SAVE_GAME_VERSION_3 3
#define SAVE_GAME_VERSION_4 4
#define SAVE_GAME_VERSION_5 5
#define SAVE_GAME_VERSION_6 6
#define SAVE_GAME_VERSION_7 7
#define SAVE_GAME_VERSION_8 8
#define SAVE_GAME_VERSION_9 9
#define SAVE_GAME_VERSION_10 10
#define SAVE_GAME_VERSION  SAVE_GAME_VERSION_10

typedef struct {
	uint8_t *address;
	uint32_t mask;
} memoryMap;

typedef union {
	struct {
#ifdef WORDS_BIGENDIAN
		uint8_t B3;
		uint8_t B2;
		uint8_t B1;
		uint8_t B0;
#else
		uint8_t B0;
		uint8_t B1;
		uint8_t B2;
		uint8_t B3;
#endif
	} B;
	struct {
#ifdef WORDS_BIGENDIAN
		uint16_t W1;
		uint16_t W0;
#else
		uint16_t W0;
		uint16_t W1;
#endif
	} W;
#ifdef WORDS_BIGENDIAN
	volatile uint32_t I;
#else
	uint32_t I;
#endif
} reg_pair;

#ifndef NO_GBA_MAP
extern memoryMap map[256];
#endif

extern uint8_t biosProtected[4];

extern uint64_t joy;
extern void (*cpuSaveGameFunc)(uint32_t,uint8_t);

extern bool CPUReadGSASnapshot(const char *);
extern bool CPUReadGSASPSnapshot(const char *);
extern bool CPUWriteGSASnapshot(const char *, const char *, const char *, const char *);
extern bool CPUWriteBatteryFile(const char *);
extern bool CPUReadBatteryFile(const char *);
extern bool CPUExportEepromFile(const char *);
extern bool CPUImportEepromFile(const char *);
extern void CPUCleanUp();
extern void CPUUpdateRender();
extern bool CPUReadMemState(char *, int);
extern bool CPUReadState(const char *);
extern bool CPUWriteMemState(char *, int);

extern bool CPUWriteState(const char *);
extern int CPULoadRom(const char *);
extern void doMirroring(bool);
extern void CPUUpdateRegister(uint32_t, uint16_t);
extern void applyTimer ();
extern void CPUInit(const char *,bool);
extern void CPUReset();

extern void CPUCheckDMA(int,int);

#ifdef USE_PNG
extern bool CPUWritePNGFile(const char *);
#endif
#ifdef USE_BMP
extern bool CPUWriteBMPFile(const char *);
#endif

#ifdef USE_FRAMESKIP
extern void CPULoop(int ticks);
#else
extern void CPULoop();
#endif

#ifdef __LIBGBA__
extern unsigned CPUWriteState_libgba(uint8_t*, unsigned);
extern bool CPUReadState_libgba(const uint8_t*, unsigned);
#endif

#ifdef PROFILING
#include "prof/prof.h"
extern void cpuProfil(profile_segment *seg);
extern void cpuEnableProfiling(int hz);
#endif

//extern struct EmulatedSystem GBASystem;

#define R13_IRQ  18
#define R14_IRQ  19
#define SPSR_IRQ 20
#define R13_USR  26
#define R14_USR  27
#define R13_SVC  28
#define R14_SVC  29
#define SPSR_SVC 30
#define R13_ABT  31
#define R14_ABT  32
#define SPSR_ABT 33
#define R13_UND  34
#define R14_UND  35
#define SPSR_UND 36
#define R8_FIQ   37
#define R9_FIQ   38
#define R10_FIQ  39
#define R11_FIQ  40
#define R12_FIQ  41
#define R13_FIQ  42
#define R14_FIQ  43
#define SPSR_FIQ 44

#ifdef USE_CHEATS
#include "Cheats.h"
#endif
#include "Globals.h"
#include "EEprom.h"
#include "Flash.h"

#endif // GBA_H
