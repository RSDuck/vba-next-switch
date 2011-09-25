// Nintendo Game Boy sound hardware emulator with save state support

// Gb_Snd_Emu 0.2.0
#ifndef GB_APU_H
#define GB_APU_H

#include "Gb_Oscs.h"

struct gb_apu_state_t;

// Sets buffer(s) to generate sound into. If left and right are NULL, output is mono.
// If all are NULL, no output is generated but other emulation still runs.
// If chan is specified, only that channel's output is changed, otherwise all are.
// 0: Square 1, 1: Square 2, 2: Wave, 3: Noise
#define OSC_COUNT 4

// Resets hardware to initial power on state BEFORE boot ROM runs. Mode selects
// sound hardware. Additional AGB wave features are enabled separately.
//mode_dmg = Game Boy monochrome
//mode_cgb = Game Boy Color
//mode_agb = Game Boy Advance
#define MODE_DMG	0
#define MODE_CGB	1
#define MODE_AGB	2

#define START_ADDR	0xFF10
#define END_ADDR	0xFF3F

// Reads and writes must be within the start_addr to end_addr range, inclusive.
// Addresses outside this range are not mapped to the sound hardware.

// register_count = END_ADDR - START_ADDR + 1 = 0xFF3F - 0xFF10 +1 = 48
#define REGISTER_COUNT 48

// regs_size = REGISTER_COUNT + 0x10 = 64
#define REGS_SIZE 64

// Treble and bass values for various hardware.

//speaker on system
#define speaker_treble -47
#define speaker_bass	2000

//headphones on each system
#define dmg_treble	0
#define dmg_bass	30
#define cgb_treble	0

//CGB has much less bass
#define cgb_bass	300
#define agb_treble	0
#define agb_bass	30

#define RUN_UNTIL(time) \
	if (time > last_time) \
	{ \
		run_until_(time); \
	}

class Gb_Apu
{
	public:
	Gb_Apu();

	// Clock rate that sound hardware runs at.
	enum { clock_rate = 4194304 * GB_APU_OVERCLOCK };

	void set_output( Blip_Buffer* center, Blip_Buffer* left = NULL, Blip_Buffer* right = NULL, int chan = OSC_COUNT );

	void reset( uint32_t mode = MODE_CGB, bool agb_wave = false );


	// Times are specified as the number of clocks since the beginning of the
	// current time frame.

	// Emulates CPU write of data to addr at specified time.
	void write_register( int32_t time, unsigned addr, int data );

	// Emulates CPU read from addr at specified time.
	int read_register( int32_t time, unsigned addr );

	// Emulates sound hardware up to specified time, ends current time frame, then
	// starts a new frame at time 0.
	void end_frame( int32_t frame_length );

	// Sound adjustments

	// Sets overall volume, where 1.0 is normal.
	void volume( float );

	// If true, reduces clicking by disabling DAC biasing. Note that this reduces
	// emulation accuracy, since the clicks are authentic.
	void reduce_clicks( bool reduce = true );

	// Save states

	// Saves full emulation state to state_out. Data format is portable and
	// includes some extra space to avoid expansion in case more state needs
	// to be stored in the future.
	void save_state( gb_apu_state_t* state_out );

	// Loads state. You should call reset() BEFORE this.
	int32_t load_state( gb_apu_state_t const& in );
	private:
	// noncopyable
	Gb_Apu( const Gb_Apu& );
	Gb_Apu& operator = ( const Gb_Apu& );

	Gb_Osc*		oscs [OSC_COUNT];
	int32_t		last_time;          // time sound emulator has been run to
	int32_t		frame_period;       // clocks between each frame sequencer step
	float		volume_;
	bool		reduce_clicks_;

	Gb_Sweep_Square square1;
	Gb_Square       square2;
	Gb_Wave         wave;
	Gb_Noise        noise;
	int32_t		frame_time;     // time of next frame sequencer action
	int             frame_phase;    // phase of next frame sequencer step
	uint8_t		regs[REGS_SIZE];// last values written to registers

	// large objects after everything else
	Blip_Synth good_synth;
	Blip_Synth med_synth;

	void apply_stereo();
	void run_until_( int32_t );
	void silence_osc( Gb_Osc& );
	void write_osc( int index, int reg, int old_data, int data );
	int32_t save_load( gb_apu_state_t*, bool save );
	void save_load2( gb_apu_state_t*, bool save );
};

#define FORMAT0 0x50414247

// Format of save state. Should be stable across versions of the library,
// with earlier versions properly opening later save states. Includes some
// room for expansion so the state size shouldn't increase.
struct gb_apu_state_t
{
	int format;   // format of all following data
	int version;  // later versions just add fields to end

	unsigned char regs [0x40];
	int frame_time;
	int frame_phase;

	int sweep_freq;
	int sweep_delay;
	int sweep_enabled;
	int sweep_neg;
	int noise_divider;
	int wave_buf;

	int delay      [4];
	int length_ctr [4];
	int phase      [4];
	int enabled    [4];

	int env_delay   [3];
	int env_volume  [3];
	int env_enabled [3];

	int unused  [13]; // for future expansion
};

#endif
