#ifndef SOUND_H
#define SOUND_H

#include "../System.h"

#define SGCNT0_H 0x82
#define FIFOA_L 0xa0
#define FIFOA_H 0xa2
#define FIFOB_L 0xa4
#define FIFOB_H 0xa6

void soundSetVolume( float );
float soundGetVolume (void);

// Manages muting bitmask. The bits control the following channels:
// 0x001 Pulse 1
// 0x002 Pulse 2
// 0x004 Wave
// 0x008 Noise
// 0x100 PCM 1
// 0x200 PCM 2
void soundSetEnable( int mask );
int  soundGetEnable (void);
void soundPause (void);
void soundResume (void);
long soundGetSampleRate (void);
void soundSetSampleRate(long sampleRate);
void soundReset (void);
void soundEvent_u8( int gb_addr, uint32_t addr, uint8_t  data );
void soundEvent_u8_parallel(int gb_addr[], uint32_t address[], uint8_t data[]);
void soundEvent_u16( uint32_t addr, uint16_t data );
void soundTimerOverflow( int which );
void process_sound_tick_fn (void);
void soundSaveGameMem(uint8_t *& data);
void soundReadGameMem(const uint8_t *& data, int version);

extern int SOUND_CLOCK_TICKS;   // Number of 16.8 MHz clocks between calls to soundTick()
extern int soundTicks;          // Number of 16.8 MHz clocks until soundTick() will be called

#endif // SOUND_H
