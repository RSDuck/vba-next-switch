// Private oscillators used by Gb_Apu

// Gb_Snd_Emu 0.2.0
#ifndef GB_OSCS_H
#define GB_OSCS_H

#include "blargg_common.h"
#include "Sound_Buffer.h"

#ifndef GB_APU_OVERCLOCK
        #define GB_APU_OVERCLOCK 1
#endif

#define	CLK_MUL	GB_APU_OVERCLOCK
#define DAC_BIAS 7
#define LENGTH_ENABLED 0x40

#define GB_OSC_CLOCK_LENGTH(osc) \
        if ( (osc.regs [4] & LENGTH_ENABLED) && osc.length_ctr) \
        { \
                if ( --osc.length_ctr <= 0 ) \
                        osc.enabled = false; \
        }

class Gb_Osc
{
	public:
	Blip_Buffer* outputs [4];	// NULL, right, left, center
	Blip_Buffer* output;		// where to output sound
	uint8_t * regs;			// osc's 5 registers
	int mode;			// MODE_DMG, MODE_CGB, MODE_AGB
	int dac_off_amp;		// amplitude when DAC is off
	int last_amp;			// current amplitude in Blip_Buffer
	Blip_Synth const* good_synth;
	Blip_Synth const* med_synth;

	int delay;			// clocks until frequency timer expires
	int length_ctr;			// length counter
	unsigned phase;			// waveform phase (or equivalent)
	bool enabled;			// internal enabled flag
	protected:
	// 11-bit frequency in NRx3 and NRx4
	int frequency() const { return (regs [4] & 7) * 0x100 + regs [3]; }
	int write_trig( int frame_phase, int max_len, int old_data );
};

// Non-zero if DAC is enabled
#define GB_ENV_DAC_ENABLED() regs[2] & 0xF8

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
		output   = 0;
		last_amp = 0;
		delay    = 0;
		phase    = 0;
		enabled  = false;
	}
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
		env_delay = 0;
		volume    = 0;
		output   = 0;
		last_amp = 0;
		delay    = 0;
		phase    = 0;
		enabled  = false;
		delay = 0x40000000; // TODO: something less hacky (never clocked until first trigger)
	}
	private:
	// Frequency timer period
	int period() const { return (2048 - frequency()) * (4 * CLK_MUL); }
};

#define	PERIOD_MASK 0x70
#define SHIFT_MASK  0x07

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
		env_delay = 0;
		volume    = 0;
		output   = 0;
		last_amp = 0;
		delay    = 0;
		phase    = 0;
		enabled  = false;
		delay = 0x40000000; // TODO: something less hacky (never clocked until first trigger)
	}
	private:
	void calc_sweep( bool update );
	void reload_sweep_timer();
};

#define	PERIOD2_MASK 0x1FFFF

#define PERIOD2_INDEX() (regs [3] >> 4)
#define PERIOD2(base) base << PERIOD2_INDEX()

class Gb_Noise : public Gb_Env
{
	public:
	int divider; // noise has more complex frequency divider setup

	void run( int32_t, int32_t );
	void write_register( int frame_phase, int reg, int old_data, int data );

	void reset()
	{
		divider = 0;
		env_delay = 0;
		volume    = 0;
		output   = 0;
		last_amp = 0;
		delay    = 0;
		phase    = 0;
		enabled  = false;
		delay = 4 * CLK_MUL; // TODO: remove?
	}
	private:
	unsigned lfsr_mask() const { return (regs [3] & 0x08) ? ~0x4040 : ~0x4000; }
};

#define BANK40_MASK	0x40
#define BANK_SIZE	32

// Non-zero if DAC is enabled
#define GBA_WAVE_DAC_ENABLED() regs[0] & 0x80

class Gb_Wave : public Gb_Osc
{
	public:
	int sample_buf;		// last wave RAM byte read (hardware has this as well)
	int agb_mask;		// 0xFF if AGB features enabled, 0 otherwise
	uint8_t* m_wave_ram;	// 32 bytes (64 nybbles), stored in APU

	void write_register( int frame_phase, int reg, int old_data, int data );
	void run( int32_t, int32_t );

	// Reads/writes wave RAM
	int read( unsigned addr ) const;
	void write( unsigned addr, int data );

	void reset()
	{
		sample_buf = 0;
		output   = 0;
		last_amp = 0;
		delay    = 0;
		phase    = 0;
		enabled  = false;
	}

	private:
	// Frequency timer period
	int period() const { return (2048 - frequency()) * (2 * CLK_MUL); }

	void corrupt_wave();

	uint8_t* wave_bank() const { return &m_wave_ram[(~regs [0] & BANK40_MASK) >> 2 & agb_mask]; }

	// Wave index that would be accessed, or -1 if no access would occur
	int access( unsigned addr ) const;
};

inline int Gb_Wave::read( unsigned addr ) const
{
	int index = access( addr );
	return (index < 0 ? 0xFF : wave_bank() [index]);
}

inline void Gb_Wave::write( unsigned addr, int data )
{
        int index = access( addr );
        if ( index >= 0 )
                wave_bank() [index] = data;;
}

#endif
