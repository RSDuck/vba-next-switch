#include <stdint.h>
#include <string.h>

#include "Sound.h"

#include "GBA.h"
#include "Globals.h"

#include "../Util.h"
#include "../common/Port.h"
#include "../System.h"

#include "../apu/Gb_Apu.h"
#include "../apu/Sound_Buffer.h"

#define NR10 0x60
#define NR11 0x62
#define NR12 0x63
#define NR13 0x64
#define NR14 0x65
#define NR21 0x68
#define NR22 0x69
#define NR23 0x6c
#define NR24 0x6d
#define NR30 0x70
#define NR31 0x72
#define NR32 0x73
#define NR33 0x74
#define NR34 0x75
#define NR41 0x78
#define NR42 0x79
#define NR43 0x7c
#define NR44 0x7d
#define NR50 0x80
#define NR51 0x81
#define NR52 0x84

/* 1/100th of a second */
#define SOUND_CLOCK_TICKS_ 167772 
#define SOUNDVOLUME 0.5f
#define SOUNDVOLUME_ -1

static int16_t   soundFinalWave [1600];
long  soundSampleRate    = 22050;
bool  soundPaused        = true;
int   SOUND_CLOCK_TICKS  = SOUND_CLOCK_TICKS_;
int   soundTicks         = SOUND_CLOCK_TICKS_;

static int soundEnableFlag   = 0x3ff; /* emulator channels enabled*/
static float const apu_vols [4] = { 0.25, 0.5, 1, 0.25 };

static const int table [0x40] =
{
		0xFF10,     0,0xFF11,0xFF12,0xFF13,0xFF14,     0,     0,
		0xFF16,0xFF17,     0,     0,0xFF18,0xFF19,     0,     0,
		0xFF1A,     0,0xFF1B,0xFF1C,0xFF1D,0xFF1E,     0,     0,
		0xFF20,0xFF21,     0,     0,0xFF22,0xFF23,     0,     0,
		0xFF24,0xFF25,     0,     0,0xFF26,     0,     0,     0,
		     0,     0,     0,     0,     0,     0,     0,     0,
		0xFF30,0xFF31,0xFF32,0xFF33,0xFF34,0xFF35,0xFF36,0xFF37,
		0xFF38,0xFF39,0xFF3A,0xFF3B,0xFF3C,0xFF3D,0xFF3E,0xFF3F,
};

class Gba_Pcm
{
	public:
		void init();
		void apply_control( int idx );
		void update( int dac );
		void end_frame( int );

	private:
		Blip_Buffer* output;
		int last_time;
		int last_amp;
		int shift;
};

class Gba_Pcm_Fifo
{
	public:
		int     which;
		Gba_Pcm pcm;

		void write_control( int data );
		void write_fifo( int data );
		void timer_overflowed( int which_timer );

		// public only so save state routines can access it
		int  readIndex;
		int  count;
		int  writeIndex;
		int  dac;
		uint8_t   fifo [32];
	private:

		int  timer;
		bool enabled;
};

static Gba_Pcm_Fifo     pcm [2];
static Gb_Apu*          gb_apu;
static Stereo_Buffer*   stereo_buffer;

static Blip_Synth pcm_synth; // 32 kHz, 16 kHz, 8 kHz

void Gba_Pcm::init()
{
	output    = 0;
	last_time = 0;
	last_amp  = 0;
	shift     = 0;
}

void Gba_Pcm::apply_control( int idx )
{
	int ch = 0;
	shift = ~ioMem [SGCNT0_H] >> (2 + idx) & 1;

	if ( (ioMem [NR52] & 0x80) )
		ch = ioMem [SGCNT0_H+1] >> (idx << 2) & 3;

	Blip_Buffer* out = 0;
	switch ( ch )
	{
		case 1:
			out = &stereo_buffer->bufs_buffer[1];
			break;
		case 2:
			out = &stereo_buffer->bufs_buffer[0];
			break;
		case 3:
			out = &stereo_buffer->bufs_buffer[2];
			break;
	}

	if ( output != out )
	{
		if ( output )
			pcm_synth.offset( SOUND_CLOCK_TICKS - soundTicks, -last_amp, output );
		last_amp = 0;
		output = out;
	}
}

void Gba_Pcm::end_frame( int time )
{
	last_time -= time;
	if ( last_time < -2048 )
		last_time = -2048;
}

void Gba_Pcm::update( int dac )
{
	if ( output )
	{
		int time = SOUND_CLOCK_TICKS -  soundTicks;

		dac = (int8_t) dac >> shift;
		int delta = dac - last_amp;
		if ( delta )
		{
			last_amp = dac;

			int filter = 0;
         #ifdef USE_SOUNDINTERPOLATION
				// base filtering on how long since last sample was output
				int period = time - last_time;

				int idx = (unsigned) period / 512;
				if ( idx >= 3 )
					idx = 3;

				static int const filters [4] = { 0, 0, 1, 2 };
				filter = filters [idx];
			//}
         #endif

			pcm_synth.offset( time, delta, output );
		}
		last_time = time;
	}
}

static void soundEvent_u16_parallel(uint32_t address[])
{
	for(int i = 0; i < 8; i++)
	{
		switch ( address[i] )
		{
			case SGCNT0_H:
				//Begin of Write SGCNT0_H
				WRITE16LE( &ioMem [SGCNT0_H], 0 & 0x770F );
				pcm [0].write_control( 0      );
				pcm [1].write_control( 0);

				//Apply Volume
				gb_apu->volume( SOUNDVOLUME_ * apu_vols [ioMem [SGCNT0_H] & 3] );
				//End of Apply Volume
				//End of SGCNT0_H
				break;

			case FIFOA_L:
			case FIFOA_H:
				pcm [0].write_fifo( 0 );
				WRITE16LE( &ioMem[address[i]], 0 );
				break;

			case FIFOB_L:
			case FIFOB_H:
				pcm [1].write_fifo( 0 );
				WRITE16LE( &ioMem[address[i]], 0 );
				break;

			case 0x88:
				WRITE16LE( &ioMem[address[i]], 0 );
				break;

			default:
				{
					int gb_addr[2]	= {address[i] & ~1, address[i] | 1};
					uint32_t address_array[2] = {address[i] & ~ 1, address[i] | 1};
					uint8_t data_array[2] = {0};
					gba_to_gb_sound_parallel(&gb_addr[0], &gb_addr[1]);
					soundEvent_u8_parallel(gb_addr, address_array, data_array);
					break;
				}
		}
	}
}

void Gba_Pcm_Fifo::timer_overflowed( int which_timer )
{
	if ( which_timer == timer && enabled )
	{
		if ( count <= 16 )
		{
			// Need to fill FIFO
			CPUCheckDMA( 3, which ? 4 : 2 );
			if ( count <= 16 )
			{
				// Not filled by DMA, so fill with 16 bytes of silence
				int reg = which ? FIFOB_L : FIFOA_L;

				uint32_t address_array[8] = {reg, reg+2, reg, reg+2, reg, reg+2, reg, reg+2};
				soundEvent_u16_parallel(address_array);
			}
		}

		// Read next sample from FIFO
		count--;
		dac = fifo [readIndex];
		readIndex = (readIndex + 1) & 31;
		pcm.update( dac );
	}
}

void Gba_Pcm_Fifo::write_control( int data )
{
	enabled = (data & 0x0300) ? true : false;
	timer   = (data & 0x0400) ? 1 : 0;

	if ( data & 0x0800 )
	{
		// Reset
		writeIndex = 0;
		readIndex  = 0;
		count      = 0;
		dac        = 0;
		memset( fifo, 0, sizeof fifo );
	}

	pcm.apply_control( which );
	pcm.update( dac );
}

void Gba_Pcm_Fifo::write_fifo( int data )
{
	fifo [writeIndex  ] = data & 0xFF;
	fifo [writeIndex+1] = data >> 8;
	count += 2;
	writeIndex = (writeIndex + 2) & 31;
}

int gba_to_gb_sound( int addr )
{
	if ( addr >= 0x60 && addr < 0xA0 )
		return table [addr - 0x60];
	return 0;
}

void gba_to_gb_sound_parallel( int * __restrict addr, int * __restrict addr2 )
{
	uint32_t addr1_table = *addr - 0x60;
	uint32_t addr2_table = *addr2 - 0x60;
	if ( *addr >= 0x60 && *addr < 0xA0 )
		*addr = table [addr1_table];
	else
		*addr = 0;
	if ( *addr2 >= 0x60 && *addr2 < 0xA0 )
		*addr2 = table [addr2_table];
	else
		*addr2 = 0;
}

void soundEvent_u8_parallel(int gb_addr[], uint32_t address[], uint8_t data[])
{
	for(uint32_t i = 0; i < 2; i++)
	{
		ioMem[address[i]] = data[i];
		gb_apu->write_register( SOUND_CLOCK_TICKS -  soundTicks, gb_addr[i], data[i] );

		if ( address[i] == NR52 )
		{
			pcm [0].pcm.apply_control( 0 );
			pcm [1].pcm.apply_control( 1 );
		}
		// TODO: what about byte writes to SGCNT0_H etc.?
	}
}

void soundEvent_u8(int gb_addr, uint32_t address, uint8_t data)
{
	ioMem[address] = data;
	gb_apu->write_register( SOUND_CLOCK_TICKS -  soundTicks, gb_addr, data );

	if ( address == NR52 )
	{
		pcm [0].pcm.apply_control( 0 );
		pcm [1].pcm.apply_control( 1 );
	}
	// TODO: what about byte writes to SGCNT0_H etc.?
}


void soundEvent_u16(uint32_t address, uint16_t data)
{
	switch ( address )
	{
		case SGCNT0_H:
			//Begin of Write SGCNT0_H
			WRITE16LE( &ioMem [SGCNT0_H], data & 0x770F );
			pcm [0].write_control( data      );
			pcm [1].write_control( data >> 4 );

			//Apply Volume
			gb_apu->volume( SOUNDVOLUME_ * apu_vols [ioMem [SGCNT0_H] & 3] );
			//End of Apply Volume
			//End of SGCNT0_H
			break;

		case FIFOA_L:
		case FIFOA_H:
			pcm [0].write_fifo( data );
			WRITE16LE( &ioMem[address], data );
			break;

		case FIFOB_L:
		case FIFOB_H:
			pcm [1].write_fifo( data );
			WRITE16LE( &ioMem[address], data );
			break;

		case 0x88:
			data &= 0xC3FF;
			WRITE16LE( &ioMem[address], data );
			break;

		default:
			{
				int gb_addr[2]	= {address & ~1, address | 1};
				uint32_t address_array[2] = {address & ~ 1, address | 1};
				uint8_t data_array[2] = {(uint8_t)data, (uint8_t)(data >> 8)};
				gba_to_gb_sound_parallel(&gb_addr[0], &gb_addr[1]);
				soundEvent_u8_parallel(gb_addr, address_array, data_array);
				break;
			}
	}
}

void soundTimerOverflow(int timer)
{
	pcm [0].timer_overflowed( timer );
	pcm [1].timer_overflowed( timer );
}

void flush_samples(Simple_Effects_Buffer * buffer)
{
	// dump all the samples available
	// VBA will only ever store 1 frame worth of samples
	int numSamples = buffer->read_samples( (int16_t*) soundFinalWave, buffer->samples_avail() );
	systemOnWriteDataToSoundBuffer(soundFinalWave, numSamples);
}

void psoundTickfn()
{
	// Run sound hardware to present
	//end_frame( SOUND_CLOCK_TICKS );
	pcm [0].pcm.end_frame( SOUND_CLOCK_TICKS );
	pcm [1].pcm.end_frame( SOUND_CLOCK_TICKS );

	gb_apu       ->end_frame( SOUND_CLOCK_TICKS );
	stereo_buffer->end_frame( SOUND_CLOCK_TICKS );

	// dump all the samples available
	// VBA will only ever store 1 frame worth of samples
	int numSamples = stereo_buffer->read_samples( (int16_t*) soundFinalWave, stereo_buffer->samples_avail() );
	systemOnWriteDataToSoundBuffer(soundFinalWave, numSamples);
}

static void apply_muting()
{
	// PCM
	pcm [0].pcm.apply_control( 0 );
	pcm [1].pcm.apply_control( 1 );

	// APU
	gb_apu->set_output( &stereo_buffer->bufs_buffer[2],
	&stereo_buffer->bufs_buffer[0], &stereo_buffer->bufs_buffer[1], 0 );

	gb_apu->set_output( &stereo_buffer->bufs_buffer[2],
	&stereo_buffer->bufs_buffer[0], &stereo_buffer->bufs_buffer[1], 1 );

	gb_apu->set_output( &stereo_buffer->bufs_buffer[2],
	&stereo_buffer->bufs_buffer[0], &stereo_buffer->bufs_buffer[1], 2 );

	gb_apu->set_output( &stereo_buffer->bufs_buffer[2],
	&stereo_buffer->bufs_buffer[0], &stereo_buffer->bufs_buffer[1], 3 );
}


static void remake_stereo_buffer()
{
	if ( !ioMem )
		return;

	// Clears pointers kept to old stereo_buffer
	pcm [0].pcm.init();
	pcm [1].pcm.init();

	// Stereo_Buffer
	delete stereo_buffer;
	stereo_buffer = 0;

	stereo_buffer = new Stereo_Buffer; // TODO: handle out of memory
	stereo_buffer->set_sample_rate( soundSampleRate ); // TODO: handle out of memory
	stereo_buffer->clock_rate( CLOCK_RATE );

	// PCM
	pcm [0].which = 0;
	pcm [1].which = 1;

	// APU
	if ( !gb_apu )
	{
		gb_apu = new Gb_Apu; // TODO: handle out of memory
		gb_apu->reset( MODE_AGB, true );

		if ( stereo_buffer )
			stereo_buffer->clear();

		soundTicks = SOUND_CLOCK_TICKS;
	}

	if ( stereo_buffer || ioMem )
	{
		apply_muting();
	}

	gb_apu->volume( SOUNDVOLUME_ * apu_vols [ioMem [SGCNT0_H] & 3] );

	pcm_synth.volume( 0.66 / 256 * SOUNDVOLUME_ );
}

void soundShutdown()
{
   systemOnSoundShutdown();
}

void soundPause()
{
	soundPaused = true;
   systemSoundPause();
}

void soundResume()
{
	soundPaused = false;
   systemSoundResume();
}


float soundGetVolume()
{
	return SOUNDVOLUME;
}


int soundGetEnable()
{
	return (soundEnableFlag & 0x30f);
}

void soundReset()
{
	systemSoundReset();

	remake_stereo_buffer();
	//Begin of Reset APU
	gb_apu->reset( MODE_AGB, true );

	if ( stereo_buffer )
		stereo_buffer->clear();

	soundTicks = SOUND_CLOCK_TICKS;
	//End of Reset APU

	soundPaused = true;
	SOUND_CLOCK_TICKS = SOUND_CLOCK_TICKS_;
	soundTicks        = SOUND_CLOCK_TICKS_;

	// Sound Event (NR52)
	int gb_addr = gba_to_gb_sound( NR52 );
	if ( gb_addr )
	{
		ioMem[NR52] = 0x80;
		gb_apu->write_register( SOUND_CLOCK_TICKS -  soundTicks, gb_addr, 0x80 );

		pcm [0].pcm.apply_control( 0 );
		pcm [1].pcm.apply_control( 1 );
	}

	// TODO: what about byte writes to SGCNT0_H etc.?
	// End of Sound Event (NR52)
}

bool soundInit()
{
	bool sound_driver_ok = systemSoundInit();
	if ( !sound_driver_ok )
		return false;

	if (systemSoundInitDriver(soundSampleRate))
		return false;

	soundPaused = true;
	return true;
}

void soundSetThrottle(unsigned short throttle)
{
   systemSoundSetThrottle(throttle);
}

void soundSetSampleRate(long sampleRate)
{
	if ( soundSampleRate != sampleRate )
	{
		if ( systemCanChangeSoundQuality() )
		{
			soundShutdown();
			soundSampleRate      = sampleRate;
			soundInit();
		}
		else
		{
			soundSampleRate      = sampleRate;
		}

		remake_stereo_buffer();
	}
}

static int dummy_state [16];

#define SKIP( type, name ) { dummy_state, sizeof (type) }

#define LOAD( type, name ) { &name, sizeof (type) }

static struct {
	gb_apu_state_t apu;

	// old state
	int soundDSBValue;
	uint8_t soundDSAValue;
} state;

// Old GBA sound state format
static variable_desc old_gba_state [] =
{
	SKIP( int, soundPaused ),
	SKIP( int, soundPlay ),
	SKIP( int, soundTicks ),
	SKIP( int, SOUND_CLOCK_TICKS ),
	SKIP( int, soundLevel1 ),
	SKIP( int, soundLevel2 ),
	SKIP( int, soundBalance ),
	SKIP( int, soundMasterOn ),
	SKIP( int, soundIndex ),
	SKIP( int, sound1On ),
	SKIP( int, sound1ATL ),
	SKIP( int, sound1Skip ),
	SKIP( int, sound1Index ),
	SKIP( int, sound1Continue ),
	SKIP( int, sound1EnvelopeVolume ),
	SKIP( int, sound1EnvelopeATL ),
	SKIP( int, sound1EnvelopeATLReload ),
	SKIP( int, sound1EnvelopeUpDown ),
	SKIP( int, sound1SweepATL ),
	SKIP( int, sound1SweepATLReload ),
	SKIP( int, sound1SweepSteps ),
	SKIP( int, sound1SweepUpDown ),
	SKIP( int, sound1SweepStep ),
	SKIP( int, sound2On ),
	SKIP( int, sound2ATL ),
	SKIP( int, sound2Skip ),
	SKIP( int, sound2Index ),
	SKIP( int, sound2Continue ),
	SKIP( int, sound2EnvelopeVolume ),
	SKIP( int, sound2EnvelopeATL ),
	SKIP( int, sound2EnvelopeATLReload ),
	SKIP( int, sound2EnvelopeUpDown ),
	SKIP( int, sound3On ),
	SKIP( int, sound3ATL ),
	SKIP( int, sound3Skip ),
	SKIP( int, sound3Index ),
	SKIP( int, sound3Continue ),
	SKIP( int, sound3OutputLevel ),
	SKIP( int, sound4On ),
	SKIP( int, sound4ATL ),
	SKIP( int, sound4Skip ),
	SKIP( int, sound4Index ),
	SKIP( int, sound4Clock ),
	SKIP( int, sound4ShiftRight ),
	SKIP( int, sound4ShiftSkip ),
	SKIP( int, sound4ShiftIndex ),
	SKIP( int, sound4NSteps ),
	SKIP( int, sound4CountDown ),
	SKIP( int, sound4Continue ),
	SKIP( int, sound4EnvelopeVolume ),
	SKIP( int, sound4EnvelopeATL ),
	SKIP( int, sound4EnvelopeATLReload ),
	SKIP( int, sound4EnvelopeUpDown ),
	LOAD( int, soundEnableFlag ),
	SKIP( int, soundControl ),
	LOAD( int, pcm [0].readIndex ),
	LOAD( int, pcm [0].count ),
	LOAD( int, pcm [0].writeIndex ),
	SKIP( uint8_t,  soundDSAEnabled ), // was bool, which was one byte on MS compiler
	SKIP( int, soundDSATimer ),
	LOAD( uint8_t [32], pcm [0].fifo ),
	LOAD( uint8_t,  state.soundDSAValue ),
	LOAD( int, pcm [1].readIndex ),
	LOAD( int, pcm [1].count ),
	LOAD( int, pcm [1].writeIndex ),
	SKIP( int, soundDSBEnabled ),
	SKIP( int, soundDSBTimer ),
	LOAD( uint8_t [32], pcm [1].fifo ),
	LOAD( int, state.soundDSBValue ),

	// skipped manually
	//LOAD( int, soundBuffer[0][0], 6*735 },
	//LOAD( int, soundFinalWave[0], 2*735 },
	{ NULL, 0 }
};

variable_desc old_gba_state2 [] =
{
	LOAD( uint8_t [0x20], state.apu.regs [0x20] ),
	SKIP( int, sound3Bank ),
	SKIP( int, sound3DataSize ),
	SKIP( int, sound3ForcedOutput ),
	{ NULL, 0 }
};

// New state format
static variable_desc gba_state [] =
{
	// PCM
	LOAD( int, pcm [0].readIndex ),
	LOAD( int, pcm [0].count ),
	LOAD( int, pcm [0].writeIndex ),
	LOAD(uint8_t[32],pcm[0].fifo ),
	LOAD( int, pcm [0].dac ),

	SKIP( int [4], room_for_expansion ),

	LOAD( int, pcm [1].readIndex ),
	LOAD( int, pcm [1].count ),
	LOAD( int, pcm [1].writeIndex ),
	LOAD(uint8_t[32],pcm[1].fifo ),
	LOAD( int, pcm [1].dac ),

	SKIP( int [4], room_for_expansion ),

	// APU
	LOAD( uint8_t [0x40], state.apu.regs ),      // last values written to registers and wave RAM (both banks)
	LOAD( int, state.apu.frame_time ),      // clocks until next frame sequencer action
	LOAD( int, state.apu.frame_phase ),     // next step frame sequencer will run

	LOAD( int, state.apu.sweep_freq ),      // sweep's internal frequency register
	LOAD( int, state.apu.sweep_delay ),     // clocks until next sweep action
	LOAD( int, state.apu.sweep_enabled ),
	LOAD( int, state.apu.sweep_neg ),       // obscure internal flag
	LOAD( int, state.apu.noise_divider ),
	LOAD( int, state.apu.wave_buf ),        // last read byte of wave RAM

	LOAD( int [4], state.apu.delay ),       // clocks until next channel action
	LOAD( int [4], state.apu.length_ctr ),
	LOAD( int [4], state.apu.phase ),       // square/wave phase, noise LFSR
	LOAD( int [4], state.apu.enabled ),     // internal enabled flag

	LOAD( int [3], state.apu.env_delay ),   // clocks until next envelope action
	LOAD( int [3], state.apu.env_volume ),
	LOAD( int [3], state.apu.env_enabled ),

	SKIP( int [13], room_for_expansion ),

	// Emulator
	LOAD( int, soundEnableFlag ),

	SKIP( int [15], room_for_expansion ),

	{ NULL, 0 }
};

// Reads and discards count bytes from in
static void skip_read( gzFile in, int count )
{
	char buf [512];

	do
	{
		int n = sizeof buf;
		if ( n > count )
			n = count;

		count -= n;
		utilGzRead( in, buf, n );
	}while ( count );
}

void soundSaveGame( gzFile out )
{
	gb_apu->save_state( &state.apu );

	// Be sure areas for expansion get written as zero
	memset( dummy_state, 0, sizeof dummy_state );

	utilWriteData( out, gba_state );
}

#ifdef __LIBSNES__
void soundSaveGameMem(uint8_t *& data)
{
   gb_apu->save_state(&state.apu);
   memset(dummy_state, 0, sizeof dummy_state);
   utilWriteDataMem(data, gba_state);
}
#endif

void soundReadGame( gzFile in, int version )
{
	// Prepare APU and default state

	//Begin of Reset APU
	gb_apu->reset( MODE_AGB, true );

	if ( stereo_buffer )
		stereo_buffer->clear();

	soundTicks = SOUND_CLOCK_TICKS;
	//End of Reset APU

	gb_apu->save_state( &state.apu );

	if ( version > SAVE_GAME_VERSION_9 )
		utilReadData( in, gba_state );
	else
	{
		//Begin of soundReadGameOld
		// Read main data
		utilReadData( in, old_gba_state );
		skip_read( in, 6*735 + 2*735 );


		// Copy APU regs
		ioMem [NR52] |= 0x80; // old sound played even when this wasn't set (power on)

		state.apu.regs [gba_to_gb_sound( NR10 ) - 0xFF10] = ioMem [NR10];
		state.apu.regs [gba_to_gb_sound( NR11 ) - 0xFF10] = ioMem [NR11];
		state.apu.regs [gba_to_gb_sound( NR12 ) - 0xFF10] = ioMem [NR12];
		state.apu.regs [gba_to_gb_sound( NR13 ) - 0xFF10] = ioMem [NR13];
		state.apu.regs [gba_to_gb_sound( NR14 ) - 0xFF10] = ioMem [NR14];
		state.apu.regs [gba_to_gb_sound( NR21 ) - 0xFF10] = ioMem [NR21];
		state.apu.regs [gba_to_gb_sound( NR22 ) - 0xFF10] = ioMem [NR22];
		state.apu.regs [gba_to_gb_sound( NR23 ) - 0xFF10] = ioMem [NR23];
		state.apu.regs [gba_to_gb_sound( NR24 ) - 0xFF10] = ioMem [NR24];
		state.apu.regs [gba_to_gb_sound( NR30 ) - 0xFF10] = ioMem [NR30];
		state.apu.regs [gba_to_gb_sound( NR31 ) - 0xFF10] = ioMem [NR31];
		state.apu.regs [gba_to_gb_sound( NR32 ) - 0xFF10] = ioMem [NR32];
		state.apu.regs [gba_to_gb_sound( NR33 ) - 0xFF10] = ioMem [NR33];
		state.apu.regs [gba_to_gb_sound( NR34 ) - 0xFF10] = ioMem [NR34];
		state.apu.regs [gba_to_gb_sound( NR41 ) - 0xFF10] = ioMem [NR41];
		state.apu.regs [gba_to_gb_sound( NR42 ) - 0xFF10] = ioMem [NR42];
		state.apu.regs [gba_to_gb_sound( NR43 ) - 0xFF10] = ioMem [NR43];
		state.apu.regs [gba_to_gb_sound( NR44 ) - 0xFF10] = ioMem [NR44];
		state.apu.regs [gba_to_gb_sound( NR50 ) - 0xFF10] = ioMem [NR50];
		state.apu.regs [gba_to_gb_sound( NR51 ) - 0xFF10] = ioMem [NR51];
		state.apu.regs [gba_to_gb_sound( NR52 ) - 0xFF10] = ioMem [NR52];

		// Copy wave RAM to both banks
		memcpy( &state.apu.regs [0x20], &ioMem [0x90], 0x10 );
		memcpy( &state.apu.regs [0x30], &ioMem [0x90], 0x10 );

		// Read both banks of wave RAM if available
		if ( version >= SAVE_GAME_VERSION_3 )
			utilReadData( in, old_gba_state2 );

		// Restore PCM
		pcm [0].dac = state.soundDSAValue;
		pcm [1].dac = state.soundDSBValue;

		(void) utilReadInt( in ); // ignore quality
		//End of soundReadGameOld
	}

	gb_apu->load_state( state.apu );
	//Begin of Write SGCNT0_H
	int data = (READ16LE( &ioMem [SGCNT0_H] ) & 0x770F);
	WRITE16LE( &ioMem [SGCNT0_H], data & 0x770F );
	pcm [0].write_control( data      );
	pcm [1].write_control( data >> 4 );

	//Apply Volume
	gb_apu->volume( SOUNDVOLUME_ * apu_vols [ioMem [SGCNT0_H] & 3] );
	//End of Apply Volume
	//End of SGCNT0_H

	//apply_muting();
}

#ifdef __LIBSNES__
void soundReadGameMem(const uint8_t *& in_data, int)
{
	// Prepare APU and default state

	//Begin of Reset APU
	gb_apu->reset( MODE_AGB, true );

	if ( stereo_buffer )
		stereo_buffer->clear();

	soundTicks = SOUND_CLOCK_TICKS;
	//End of Reset APU

	gb_apu->save_state( &state.apu );

	utilReadDataMem( in_data, gba_state );

	gb_apu->load_state( state.apu );
	//Begin of Write SGCNT0_H
	int data = (READ16LE( &ioMem [SGCNT0_H] ) & 0x770F);
	WRITE16LE( &ioMem [SGCNT0_H], data & 0x770F );
	pcm [0].write_control( data      );
	pcm [1].write_control( data >> 4 );

	//Apply Volume
	gb_apu->volume( SOUNDVOLUME_ * apu_vols [ioMem [SGCNT0_H] & 3] );
	//End of Apply Volume
	//End of SGCNT0_H

	//apply_muting();
}
#endif
