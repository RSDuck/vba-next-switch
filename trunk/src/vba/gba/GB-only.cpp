#ifndef USE_GBA_ONLY
#ifdef USE_GB_ONLY
#include <stdlib.h>

#ifdef __LIBSNES__
#include <stddef.h>
#endif

#include "Sound.h"
#include "bios.h"
#include "../NLS.h"
#include "../Util.h"
#include "../common/Port.h"
#include "../System.h"
#ifdef USE_SFML
#include "GBALink.h"
#endif

#ifdef PROFILING
#include "prof/prof.h"
#endif

#ifdef __GNUC__
#define _stricmp strcasecmp
#include <string.h>
#include <stdio.h>
#endif

int coeff[32] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};

bool ioReadable[0x400];
bool N_FLAG = 0;
bool C_FLAG = 0;
bool Z_FLAG = 0;
bool V_FLAG = 0;
bool armState = true;
bool armIrqEnable = true;
uint32_t armNextPC = 0x00000000;
int armMode = 0x1f;
uint32_t stop = 0x08000568;
int saveType = 0;
bool useBios = false;
bool skipBios = false;
#ifdef USE_FRAMESKIP
int frameSkip = 1;
bool speedup = false;
#endif
//bool synchronize = true;
//bool cpuDisableSfx = false;
bool cpuIsMultiBoot = false;
bool parseDebug = true;
int layerSettings = 0xff00;
int layerEnable = 0xff00;
bool speedHack = true;
int cpuSaveType = 0;
#ifdef USE_CHEATS
bool cheatsEnabled = true;
#else
bool cheatsEnabled = false;
#endif
bool enableRtc = false;
bool mirroringEnable = false;
bool skipSaveGameBattery = false;
bool skipSaveGameCheats = false;

// this is an optional hack to change the backdrop/background color:
// -1: disabled
// 0x0000 to 0x7FFF: set custom 15 bit color
//int customBackdropColor = -1;

uint8_t *bios = 0;
uint8_t *rom = 0;
uint8_t *internalRAM = 0;
uint8_t *workRAM = 0;
uint8_t *paletteRAM = 0;
uint8_t *vram = 0;
uint8_t *pix = 0;
uint8_t *oam = 0;
uint8_t *ioMem = 0;

uint16_t DISPCNT  = 0x0080;
uint16_t DISPSTAT = 0x0000;
uint16_t VCOUNT   = 0x0000;
uint16_t BG0CNT   = 0x0000;
uint16_t BG1CNT   = 0x0000;
uint16_t BG2CNT   = 0x0000;
uint16_t BG3CNT   = 0x0000;
uint16_t BG0HOFS  = 0x0000;
uint16_t BG0VOFS  = 0x0000;
uint16_t BG1HOFS  = 0x0000;
uint16_t BG1VOFS  = 0x0000;
uint16_t BG2HOFS  = 0x0000;
uint16_t BG2VOFS  = 0x0000;
uint16_t BG3HOFS  = 0x0000;
uint16_t BG3VOFS  = 0x0000;
uint16_t BG2PA    = 0x0100;
uint16_t BG2PB    = 0x0000;
uint16_t BG2PC    = 0x0000;
uint16_t BG2PD    = 0x0100;
uint16_t BG2X_L   = 0x0000;
uint16_t BG2X_H   = 0x0000;
uint16_t BG2Y_L   = 0x0000;
uint16_t BG2Y_H   = 0x0000;
uint16_t BG3PA    = 0x0100;
uint16_t BG3PB    = 0x0000;
uint16_t BG3PC    = 0x0000;
uint16_t BG3PD    = 0x0100;
uint16_t BG3X_L   = 0x0000;
uint16_t BG3X_H   = 0x0000;
uint16_t BG3Y_L   = 0x0000;
uint16_t BG3Y_H   = 0x0000;
uint16_t WIN0H    = 0x0000;
uint16_t WIN1H    = 0x0000;
uint16_t WIN0V    = 0x0000;
uint16_t WIN1V    = 0x0000;
uint16_t WININ    = 0x0000;
uint16_t WINOUT   = 0x0000;
uint16_t MOSAIC   = 0x0000;
uint16_t BLDMOD   = 0x0000;
uint16_t COLEV    = 0x0000;
uint16_t COLY     = 0x0000;
uint16_t DM0SAD_L = 0x0000;
uint16_t DM0SAD_H = 0x0000;
uint16_t DM0DAD_L = 0x0000;
uint16_t DM0DAD_H = 0x0000;
uint16_t DM0CNT_L = 0x0000;
uint16_t DM0CNT_H = 0x0000;
uint16_t DM1SAD_L = 0x0000;
uint16_t DM1SAD_H = 0x0000;
uint16_t DM1DAD_L = 0x0000;
uint16_t DM1DAD_H = 0x0000;
uint16_t DM1CNT_L = 0x0000;
uint16_t DM1CNT_H = 0x0000;
uint16_t DM2SAD_L = 0x0000;
uint16_t DM2SAD_H = 0x0000;
uint16_t DM2DAD_L = 0x0000;
uint16_t DM2DAD_H = 0x0000;
uint16_t DM2CNT_L = 0x0000;
uint16_t DM2CNT_H = 0x0000;
uint16_t DM3SAD_L = 0x0000;
uint16_t DM3SAD_H = 0x0000;
uint16_t DM3DAD_L = 0x0000;
uint16_t DM3DAD_H = 0x0000;
uint16_t DM3CNT_L = 0x0000;
uint16_t DM3CNT_H = 0x0000;
uint16_t TM0D     = 0x0000;
uint16_t TM0CNT   = 0x0000;
uint16_t TM1D     = 0x0000;
uint16_t TM1CNT   = 0x0000;
uint16_t TM2D     = 0x0000;
uint16_t TM2CNT   = 0x0000;
uint16_t TM3D     = 0x0000;
uint16_t TM3CNT   = 0x0000;
uint16_t P1       = 0xFFFF;
uint16_t IE       = 0x0000;
uint16_t IF       = 0x0000;
uint16_t IME      = 0x0000;

//END OF GLOBALS.CPP

//extern int emulating;

#ifdef USE_SWITICKS
int SWITicks = 0;
#endif
int IRQTicks = 0;

uint32_t mastercode = 0;
int layerEnableDelay = 0;
bool busPrefetch = false;
bool busPrefetchEnable = false;
uint32_t busPrefetchCount = 0;
int cpuDmaTicksToUpdate = 0;
int cpuDmaCount = 0;
//bool cpuDmaHack = false;
//uint32_t cpuDmaLast = 0;
int dummyAddress = 0;

bool cpuBreakLoop = false;
int cpuNextEvent = 0;

int gbaSaveType = 0; // used to remember the save type on reset
bool intState = false;
bool stopState = false;
bool holdState = false;
int holdType = 0;
bool cpuSramEnabled = true;
bool cpuFlashEnabled = true;
bool cpuEEPROMEnabled = true;
bool cpuEEPROMSensorEnabled = false;

uint32_t cpuPrefetch[2];

int cpuTotalTicks = 0;
#ifdef PROFILING
int profilingTicks = 0;
int profilingTicksReload = 0;
static profile_segment *profilSegment = NULL;
#endif

int lcdTicks = (useBios && !skipBios) ? 1008 : 208;
uint8_t timerOnOffDelay = 0;
uint16_t timer0Value = 0;
bool timer0On = false;
int timer0Ticks = 0;
int timer0Reload = 0;
int timer0ClockReload  = 0;
uint16_t timer1Value = 0;
bool timer1On = false;
int timer1Ticks = 0;
int timer1Reload = 0;
int timer1ClockReload  = 0;
uint16_t timer2Value = 0;
bool timer2On = false;
int timer2Ticks = 0;
int timer2Reload = 0;
int timer2ClockReload  = 0;
uint16_t timer3Value = 0;
bool timer3On = false;
int timer3Ticks = 0;
int timer3Reload = 0;
int timer3ClockReload  = 0;
uint32_t dma0Source = 0;
uint32_t dma0Dest = 0;
uint32_t dma1Source = 0;
uint32_t dma1Dest = 0;
uint32_t dma2Source = 0;
uint32_t dma2Dest = 0;
uint32_t dma3Source = 0;
uint32_t dma3Dest = 0;
#ifdef USE_FRAMESKIP
int frameCount = 0;
#endif
char buffer[1024];
FILE *out = NULL;
#ifdef USE_FRAMESKIP
uint32_t lastTime = 0;
#endif
int count = 0;

const uint32_t TIMER_TICKS[4] = {0, 6, 8, 10};

const uint32_t  objTilesAddress [3] = {0x010000, 0x014000, 0x014000};
const uint8_t gamepakRamWaitState[4] = { 4, 3, 2, 8 };
const uint8_t gamepakWaitState[4] =  { 4, 3, 2, 8 };
const uint8_t gamepakWaitState0[2] = { 2, 1 };
const uint8_t gamepakWaitState1[2] = { 4, 1 };
const uint8_t gamepakWaitState2[2] = { 8, 1 };
const bool isInRom [16]=
  { false, false, false, false, false, false, false, false,
    true, true, true, true, true, true, false, false };

uint8_t memoryWait[16] =
  { 0, 0, 2, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 0 };
uint8_t memoryWait32[16] =
  { 0, 0, 5, 0, 0, 1, 1, 0, 7, 7, 9, 9, 13, 13, 4, 0 };
uint8_t memoryWaitSeq[16] =
  { 0, 0, 2, 0, 0, 0, 0, 0, 2, 2, 4, 4, 8, 8, 4, 0 };
uint8_t memoryWaitSeq32[16] =
  { 0, 0, 5, 0, 0, 1, 1, 0, 5, 5, 9, 9, 17, 17, 4, 0 };

// The videoMemoryWait constants are used to add some waitstates
// if the opcode access video memory data outside of vblank/hblank
// It seems to happen on only one ticks for each pixel.
// Not used for now (too problematic with current code).
//const uint8_t videoMemoryWait[16] =
//  {0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};


uint8_t biosProtected[4];

#ifdef WORDS_BIGENDIAN
bool cpuBiosSwapped = false;
#endif

uint32_t myROM[] = {
0xEA000006,
0xEA000093,
0xEA000006,
0x00000000,
0x00000000,
0x00000000,
0xEA000088,
0x00000000,
0xE3A00302,
0xE1A0F000,
0xE92D5800,
0xE55EC002,
0xE28FB03C,
0xE79BC10C,
0xE14FB000,
0xE92D0800,
0xE20BB080,
0xE38BB01F,
0xE129F00B,
0xE92D4004,
0xE1A0E00F,
0xE12FFF1C,
0xE8BD4004,
0xE3A0C0D3,
0xE129F00C,
0xE8BD0800,
0xE169F00B,
0xE8BD5800,
0xE1B0F00E,
0x0000009C,
0x0000009C,
0x0000009C,
0x0000009C,
0x000001F8,
0x000001F0,
0x000000AC,
0x000000A0,
0x000000FC,
0x00000168,
0xE12FFF1E,
0xE1A03000,
0xE1A00001,
0xE1A01003,
0xE2113102,
0x42611000,
0xE033C040,
0x22600000,
0xE1B02001,
0xE15200A0,
0x91A02082,
0x3AFFFFFC,
0xE1500002,
0xE0A33003,
0x20400002,
0xE1320001,
0x11A020A2,
0x1AFFFFF9,
0xE1A01000,
0xE1A00003,
0xE1B0C08C,
0x22600000,
0x42611000,
0xE12FFF1E,
0xE92D0010,
0xE1A0C000,
0xE3A01001,
0xE1500001,
0x81A000A0,
0x81A01081,
0x8AFFFFFB,
0xE1A0000C,
0xE1A04001,
0xE3A03000,
0xE1A02001,
0xE15200A0,
0x91A02082,
0x3AFFFFFC,
0xE1500002,
0xE0A33003,
0x20400002,
0xE1320001,
0x11A020A2,
0x1AFFFFF9,
0xE0811003,
0xE1B010A1,
0xE1510004,
0x3AFFFFEE,
0xE1A00004,
0xE8BD0010,
0xE12FFF1E,
0xE0010090,
0xE1A01741,
0xE2611000,
0xE3A030A9,
0xE0030391,
0xE1A03743,
0xE2833E39,
0xE0030391,
0xE1A03743,
0xE2833C09,
0xE283301C,
0xE0030391,
0xE1A03743,
0xE2833C0F,
0xE28330B6,
0xE0030391,
0xE1A03743,
0xE2833C16,
0xE28330AA,
0xE0030391,
0xE1A03743,
0xE2833A02,
0xE2833081,
0xE0030391,
0xE1A03743,
0xE2833C36,
0xE2833051,
0xE0030391,
0xE1A03743,
0xE2833CA2,
0xE28330F9,
0xE0000093,
0xE1A00840,
0xE12FFF1E,
0xE3A00001,
0xE3A01001,
0xE92D4010,
0xE3A03000,
0xE3A04001,
0xE3500000,
0x1B000004,
0xE5CC3301,
0xEB000002,
0x0AFFFFFC,
0xE8BD4010,
0xE12FFF1E,
0xE3A0C301,
0xE5CC3208,
0xE15C20B8,
0xE0110002,
0x10222000,
0x114C20B8,
0xE5CC4208,
0xE12FFF1E,
0xE92D500F,
0xE3A00301,
0xE1A0E00F,
0xE510F004,
0xE8BD500F,
0xE25EF004,
0xE59FD044,
0xE92D5000,
0xE14FC000,
0xE10FE000,
0xE92D5000,
0xE3A0C302,
0xE5DCE09C,
0xE35E00A5,
0x1A000004,
0x05DCE0B4,
0x021EE080,
0xE28FE004,
0x159FF018,
0x059FF018,
0xE59FD018,
0xE8BD5000,
0xE169F00C,
0xE8BD5000,
0xE25EF004,
0x03007FF0,
0x09FE2000,
0x09FFC000,
0x03007FE0
};

static int romSize = 0x2000000;

#ifdef PROFILING
void cpuProfil(profile_segment *seg)
{
	profilSegment = seg;
}

void cpuEnableProfiling(int hz)
{
	if(hz == 0)
		hz = 100;
	profilingTicks = profilingTicksReload = 16777216 / hz;
	profSetHertz(hz);
}
#endif

bool CPUIsGBABios(const char * file)
{
	if(strlen(file) > 4) {
		const char * p = strrchr(file,'.');

		if(p != NULL) {
			if(_stricmp(p, ".gba") == 0)
				return true;
			if(_stricmp(p, ".agb") == 0)
				return true;
			if(_stricmp(p, ".bin") == 0)
				return true;
			if(_stricmp(p, ".bios") == 0)
				return true;
			if(_stricmp(p, ".rom") == 0)
				return true;
		}
	}

	return false;
}

#define CPUSwap(a, b) \
{ \
volatile uint32_t c = *b; \
*b = *a; \
*a = c; \
}

uint8_t cpuBitsSet[256];
uint8_t cpuLowestBitSet[256];
#endif
#endif
