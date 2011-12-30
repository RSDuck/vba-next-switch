/* Nintendo Game Boy sound hardware emulator with save state support */

/* Gb_Snd_Emu 0.2.0*/
#ifndef GB_APU_H
#define GB_APU_H

#include "blargg_common.h"
#include "blargg_source.h"
#include "Sound_Buffer.h"

#ifndef GB_APU_OVERCLOCK
        #define GB_APU_OVERCLOCK 1
#endif

#if GB_APU_OVERCLOCK & (GB_APU_OVERCLOCK - 1)
        #error "GB_APU_OVERCLOCK must be a power of 2"
#endif

#define	clk_mul	GB_APU_OVERCLOCK
#define dac_bias 7

class Gb_Osc
{
	public:
	Blip_Buffer* outputs [4];	/* NULL, right, left, center*/
	Blip_Buffer* output;		/* where to output sound*/
	uint8_t * regs;			/* osc's 5 registers*/
	int mode;			/* mode_dmg, mode_cgb, mode_agb*/
	int dac_off_amp;		/* amplitude when DAC is off*/
	int last_amp;			/* current amplitude in Blip_Buffer*/
	typedef Blip_Synth<blip_good_quality,1> Good_Synth;
	typedef Blip_Synth<blip_med_quality ,1> Med_Synth;
	Good_Synth const* good_synth;
	Med_Synth  const* med_synth;

	int delay;			/* clocks until frequency timer expires*/
	int length_ctr;			/* length counter*/
	unsigned phase;			/* waveform phase (or equivalent)*/
	bool enabled;			/* internal enabled flag*/

	void clock_length();
	void reset();
	protected:

	/* 11-bit frequency in NRx3 and NRx4*/
	int frequency() const { return (regs [4] & 7) * 0x100 + regs [3]; }

	void update_amp( int32_t, int new_amp );
	int write_trig( int frame_phase, int max_len, int old_data );
};

class Gb_Env : public Gb_Osc
{
	public:
	int  env_delay;
	int  volume;
	bool env_enabled;

	void clock_envelope();
	bool write_register( int frame_phase, int reg, int old_data, int data );

	void reset()
	{
		env_delay = 0;
		volume    = 0;
		Gb_Osc::reset();
	}
	protected:
	/* Non-zero if DAC is enabled*/
	int dac_enabled() const { return regs [2] & 0xF8; }
	private:
	void zombie_volume( int old, int data );
	int reload_env_timer();
};

class Gb_Square : public Gb_Env
{
	public:
	bool write_register( int frame_phase, int reg, int old_data, int data );
	void run( int32_t, int32_t );

	void reset()
	{
		Gb_Env::reset();
		delay = 0x40000000; /* TODO: something less hacky (never clocked until first trigger)*/
	}
	private:
	/* Frequency timer period*/
	int period() const { return (2048 - frequency()) * (4 * clk_mul); }
};

class Gb_Sweep_Square : public Gb_Square
{
	public:
	int  sweep_freq;
	int  sweep_delay;
	bool sweep_enabled;
	bool sweep_neg;

	void clock_sweep();
	void write_register( int frame_phase, int reg, int old_data, int data );

	void reset()
	{
		sweep_freq    = 0;
		sweep_delay   = 0;
		sweep_enabled = false;
		sweep_neg     = false;
		Gb_Square::reset();
	}
	private:
	enum { period_mask = 0x70 };
	enum { shift_mask  = 0x07 };

	void calc_sweep( bool update );
	void reload_sweep_timer();
};

class Gb_Noise : public Gb_Env
{
	public:
	int divider; /* noise has more complex frequency divider setup*/

	void run( int32_t, int32_t );
	void write_register( int frame_phase, int reg, int old_data, int data );

	void reset()
	{
		divider = 0;
		Gb_Env::reset();
		delay = 4 * clk_mul; /* TODO: remove?*/
	}
	private:
	enum { period2_mask = 0x1FFFF };

	int period2_index() const { return regs [3] >> 4; }
	int period2( int base = 8 ) const { return base << period2_index(); }
	unsigned lfsr_mask() const { return (regs [3] & 0x08) ? ~0x4040 : ~0x4000; }
};

class Gb_Wave : public Gb_Osc
{
	public:
	int sample_buf;		/* last wave RAM byte read (hardware has this as well)*/
	int agb_mask;		/* 0xFF if AGB features enabled, 0 otherwise*/
	uint8_t* wave_ram;	/* 32 bytes (64 nybbles), stored in APU*/

	void write_register( int frame_phase, int reg, int old_data, int data );
	void run( int32_t, int32_t );

	/* Reads/writes wave RAM*/
	int read( unsigned addr ) const;
	void write( unsigned addr, int data );

	void reset()
	{
		sample_buf = 0;
		Gb_Osc::reset();
	}

	private:
	enum { bank40_mask = 0x40 };
	enum { bank_size   = 32 };

	friend class Gb_Apu;

	/* Frequency timer period*/
	int period() const { return (2048 - frequency()) * (2 * clk_mul); }

	/* Non-zero if DAC is enabled*/
	int dac_enabled() const { return regs [0] & 0x80; }

	void corrupt_wave();

	/* Wave index that would be accessed, or -1 if no access would occur*/
	int access( unsigned addr ) const;
};

INLINE int Gb_Wave::read( unsigned addr ) const
{
	int index;

	if(enabled)
		index = access( addr );
	else
		index = addr & 0x0F;
	
	unsigned char const * wave_bank = &wave_ram[(~regs[0] & bank40_mask) >> 2 & agb_mask];

	return (index < 0 ? 0xFF : wave_bank[index]);
}

INLINE void Gb_Wave::write( unsigned addr, int data )
{
	int index;

	if(enabled)
		index = access( addr );
	else
		index = addr & 0x0F;
	
	unsigned char * wave_bank = &wave_ram[(~regs[0] & bank40_mask) >> 2 & agb_mask];

	if ( index >= 0 )
		wave_bank[index] = data;;
}

struct gb_apu_state_t;

#define osc_count 4

/* Resets hardware to initial power on state BEFORE boot ROM runs. Mode selects*/
/* sound hardware. Additional AGB wave features are enabled separately.*/
/*mode_dmg = Game Boy monochrome*/
/*mode_cgb = Game Boy Color*/
/*mode_agb = Game Boy Advance*/
#define mode_dmg	0
#define mode_cgb	1
#define mode_agb	2

class Gb_Apu
{
	public:
	/* Clock rate that sound hardware runs at.*/
	enum { clock_rate = 4194304 * GB_APU_OVERCLOCK };

	/* Sets buffer(s) to generate sound into. If left and right are NULL, output is mono.*/
	/* If all are NULL, no output is generated but other emulation still runs.*/
	/* If chan is specified, only that channel's output is changed, otherwise all are.*/
	/*enum { osc_count = 4 }; // 0: Square 1, 1: Square 2, 2: Wave, 3: Noise*/
	void set_output( Blip_Buffer* center, Blip_Buffer* left = NULL, Blip_Buffer* right = NULL,
			int chan = osc_count );

	void reset( uint32_t mode = mode_cgb, bool agb_wave = false );

	/* Reads and writes must be within the start_addr to end_addr range, inclusive.*/
	/* Addresses outside this range are not mapped to the sound hardware.*/
	enum { start_addr = 0xFF10 };
	enum { end_addr   = 0xFF3F };
	enum { register_count = end_addr - start_addr + 1 };

	/* Times are specified as the number of clocks since the beginning of the*/
	/* current time frame.*/

	/* Emulates CPU write of data to addr at specified time.*/
	void write_register( int32_t time, unsigned addr, int data );

	/* Emulates CPU read from addr at specified time.*/
	int read_register( int32_t time, unsigned addr );

	/* Emulates sound hardware up to specified time, ends current time frame, then*/
	/* starts a new frame at time 0.*/
	void end_frame( int32_t frame_length );

	/* Sound adjustments*/

	/* Sets overall volume, where 1.0 is normal.*/
	void volume( double );

	/* If true, reduces clicking by disabling DAC biasing. Note that this reduces*/
	/* emulation accuracy, since the clicks are authentic.*/
	void reduce_clicks( bool reduce = true );

	/* Treble and bass values for various hardware.*/
	enum {
		speaker_treble =  -47, /* speaker on system*/
		speaker_bass   = 2000,
		dmg_treble     =    0, /* headphones on each system*/
		dmg_bass       =   30,
		cgb_treble     =    0,
		cgb_bass       =  300, /* CGB has much less bass*/
		agb_treble     =    0,
		agb_bass       =   30
	};

	/* Save states*/

	/* Saves full emulation state to state_out. Data format is portable and*/
	/* includes some extra space to avoid expansion in case more state needs*/
	/* to be stored in the future.*/
	void save_state( gb_apu_state_t* state_out );

	/* Loads state. You should call reset() BEFORE this.*/
	const char * load_state( gb_apu_state_t const& in );

	public:
	Gb_Apu();
	private:
	/* noncopyable*/
	Gb_Apu( const Gb_Apu& );
	Gb_Apu& operator = ( const Gb_Apu& );

	Gb_Osc*     oscs [osc_count];
	int32_t last_time;          /* time sound emulator has been run to*/
	int32_t frame_period;       /* clocks between each frame sequencer step*/
	double      volume_;
	bool        reduce_clicks_;

	Gb_Sweep_Square square1;
	Gb_Square       square2;
	Gb_Wave         wave;
	Gb_Noise        noise;
	int32_t     frame_time;     /* time of next frame sequencer action*/
	int             frame_phase;    /* phase of next frame sequencer step*/
	enum { regs_size = register_count + 0x10 };
	uint8_t  regs [regs_size];/* last values written to registers*/

	/* large objects after everything else*/
	Gb_Osc::Good_Synth  good_synth;
	Gb_Osc::Med_Synth   med_synth;

	int calc_output( int osc ) const;
	void apply_stereo();
	void apply_volume();
	void synth_volume( int );
	void run_until_( int32_t );
	void run_until( int32_t );
	void silence_osc( Gb_Osc& );
	void write_osc( int index, int reg, int old_data, int data );
	const char* save_load( gb_apu_state_t*, bool save );
	void save_load2( gb_apu_state_t*, bool save );
};

/* Format of save state. Should be stable across versions of the library,*/
/* with earlier versions properly opening later save states. Includes some*/
/* room for expansion so the state size shouldn't increase.*/
struct gb_apu_state_t
{
#if GB_APU_CUSTOM_STATE
	/* Values stored as plain int so your code can read/write them easily.*/
	/* Structure can NOT be written to disk, since format is not portable.*/
	typedef int val_t;
#else
	/* Values written in portable little-endian format, allowing structure*/
	/* to be written directly to disk.*/
	typedef unsigned char val_t [4];
#endif

	enum { format0 = 0x50414247 };

	val_t format;   /* format of all following data*/
	val_t version;  /* later versions just add fields to end*/

	unsigned char regs [0x40];
	val_t frame_time;
	val_t frame_phase;

	val_t sweep_freq;
	val_t sweep_delay;
	val_t sweep_enabled;
	val_t sweep_neg;
	val_t noise_divider;
	val_t wave_buf;

	val_t delay      [4];
	val_t length_ctr [4];
	val_t phase      [4];
	val_t enabled    [4];

	val_t env_delay   [3];
	val_t env_volume  [3];
	val_t env_enabled [3];

	val_t unused  [13]; /* for future expansion*/
};

#endif
