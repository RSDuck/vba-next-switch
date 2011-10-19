// Gb_Snd_Emu 0.2.0. http://www.slack.net/~ant/

#include "Gb_Apu.h"

#include "../System.h"

/* Copyright (C) 2003-2007 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#include "blargg_source.h"

#define VOL_REG 0xFF24
#define STEREO_REG 0xFF25
#define STATUS_REG 0xFF26
#define WAVE_RAM 0xFF30

#define POWER_MASK 0x80

#define OSC_COUNT 4

#define CALC_OUTPUT(osc) \
	int bits = regs [STEREO_REG - START_ADDR] >> osc; \
	bits = (bits >> 3 & 2) | (bits & 1);

#define SYNTH_VOLUME(iv) \
{ \
	float v = volume_ * 0.60 / OSC_COUNT / 15 /*steps*/ / 8 /*master vol range*/ * iv; \
	BLIP_SYNTH_VOLUME_UNIT(good_synth.delta_factor, v * 1.0 ); \
	BLIP_SYNTH_VOLUME_UNIT(med_synth.delta_factor, v * 1.0 ); \
}

#define apply_volume() \
{ \
	/* TODO: Doesn't handle differing left and right volumes (panning). */ \
	/* Not worth the complexity. */ \
	int data  = regs [VOL_REG - START_ADDR]; \
	int left  = data >> 4 & 7; \
	int right = data & 7; \
	int iv = max(left, right) + 1; \
	SYNTH_VOLUME(iv); \
}

void Gb_Apu::set_output( Blip_Buffer* center, Blip_Buffer* left, Blip_Buffer* right, int osc )
{
	int i = osc;
	do
	{
		Gb_Osc& o = *oscs [i];
		o.outputs [1] = right;
		o.outputs [2] = left;
		o.outputs [3] = center;
		CALC_OUTPUT(i);
		o.output = o.outputs [bits];
		++i;
	}while ( i < osc );
}


void Gb_Apu::volume(float v )
{
	if ( volume_ != v )
	{
		volume_ = v;
		apply_volume();
	}
}

void Gb_Apu::reduce_clicks( bool reduce )
{
	reduce_clicks_ = reduce;

	// Click reduction makes DAC off generate same output as volume 0
	int dac_off_amp = 0;
	#ifndef USE_GBA_ONLY
	if ( reduce && wave.mode != MODE_AGB ) // AGB already eliminates clicks
		dac_off_amp = -DAC_BIAS;
	#endif

	oscs [0]->dac_off_amp = dac_off_amp;
	oscs [1]->dac_off_amp = dac_off_amp;
	oscs [2]->dac_off_amp = dac_off_amp;
	oscs [3]->dac_off_amp = dac_off_amp;

	// AGB always eliminates clicks on wave channel using same method
	#ifndef USE_GBA_ONLY
	if ( wave.mode == MODE_AGB )
	#endif
		wave.dac_off_amp = -DAC_BIAS;
}

// Load initial wave RAM
static uint8_t const initial_wave[2][16] = {
	{0x84,0x40,0x43,0xAA,0x2D,0x78,0x92,0x3C,0x60,0x59,0x59,0xB0,0x34,0xB8,0x2E,0xDA},
	{0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF},
};

void Gb_Apu::reset( uint32_t mode, bool agb_wave )
{
	// Hardware mode
	#ifndef USE_GBA_ONLY
	if ( agb_wave )
	#endif
		mode = MODE_AGB; // using AGB wave features implies AGB hardware
	wave.agb_mask = agb_wave ? 0xFF : 0;
	#ifndef USE_GBA_ONLY
	oscs [0]->mode = mode;
	oscs [1]->mode = mode;
	oscs [2]->mode = mode;
	oscs [3]->mode = mode;
	#endif
	reduce_clicks( reduce_clicks_ );

	// Reset state
	frame_time  = 0;
	last_time   = 0;
	frame_phase = 0;

	for ( int i = 0; i < 0x20; i++ )
		regs [i] = 0;

	square1.reset();
	square2.reset();
	wave   .reset();
	noise  .reset();

	apply_volume();

	square1.length_ctr = 64;
	square2.length_ctr = 64;
	wave   .length_ctr = 256;
	noise  .length_ctr = 64;

	for ( int b = 2; --b >= 0; )
	{
		// Init both banks (does nothing if not in AGB mode)
		// TODO: verify that this works
		write_register( 0, 0xFF1A, b * 0x40 );
		for ( uint32_t i = 0; i < sizeof(initial_wave[0]); i++ )
			#ifdef USE_GBA_ONLY
			write_register( 0, i + WAVE_RAM, initial_wave [1] [i] );
			#else
			write_register( 0, i + WAVE_RAM, initial_wave [(mode != MODE_DMG)] [i] );
			#endif
	}
}

Gb_Apu::Gb_Apu()
{
	wave.m_wave_ram = &regs [WAVE_RAM - START_ADDR];

	oscs [0] = &square1;
	oscs [1] = &square2;
	oscs [2] = &wave;
	oscs [3] = &noise;

	for ( int i = OSC_COUNT; --i >= 0; )
	{
		Gb_Osc& o = *oscs [i];
		o.regs        = &regs [i * 5];
		o.output      = 0;
		o.outputs [0] = 0;
		o.outputs [1] = 0;
		o.outputs [2] = 0;
		o.outputs [3] = 0;
		o.good_synth  = &good_synth;
		o.med_synth   = &med_synth;
	}

	reduce_clicks_ = false;
	//begin set Tempo ( 1.0)
	frame_period = 4194304 / 512; // 512 Hz
	//end set Tempo ( 1.0)

	volume_ = 1.0;
	reset();
}

void Gb_Apu::run_until_( int32_t end_time )
{
	do
	{
		// run oscillators
		int32_t time = end_time;
		if ( time > frame_time )
			time = frame_time;

		square1.run( last_time, time );
		square2.run( last_time, time );
		wave   .run( last_time, time );
		noise  .run( last_time, time );
		last_time = time;

		if ( time == end_time )
			break;

		// run frame sequencer
		frame_time += frame_period * CLK_MUL;
		switch ( frame_phase++ )
		{
			case 2:
			case 6:
				// 128 Hz
				square1.clock_sweep();
			case 0:
			case 4:
				// 256 Hz
				GB_OSC_CLOCK_LENGTH(square1);
				GB_OSC_CLOCK_LENGTH(square2);
				GB_OSC_CLOCK_LENGTH(wave);
				GB_OSC_CLOCK_LENGTH(noise);
				break;

			case 7:
				// 64 Hz
				frame_phase = 0;
				square1.clock_envelope();
				square2.clock_envelope();
				noise  .clock_envelope();
		}
	}while(1);
}

void Gb_Apu::end_frame( int32_t end_time )
{
	RUN_UNTIL(end_time);

	frame_time -= end_time;

	last_time -= end_time;
}

void Gb_Apu::silence_osc( Gb_Osc& o )
{
	int delta = -o.last_amp;
	if ( delta )
	{
		o.last_amp = 0;
		if ( o.output )
		{
			offset_resampled(med_synth.delta_factor, last_time * o.output->factor_ + o.output->offset_, delta, o.output );
		}
	}
}

void Gb_Apu::write_register( int32_t time, unsigned addr, int data )
{
	int reg = addr - START_ADDR;
	if ( (unsigned) reg >= REGISTER_COUNT)
		return;

	if ( addr < STATUS_REG && !(regs [STATUS_REG - START_ADDR] & POWER_MASK) )
	{
		// Power is off

		// length counters can only be written in DMG mode
		#ifdef USE_GBA_ONLY
		if ((reg != 1 && reg != 5+1 && reg != 10+1 && reg != 15+1) )
		#else
		if ( wave.mode != MODE_DMG || (reg != 1 && reg != 5+1 && reg != 10+1 && reg != 15+1) )
		#endif
			return;

		if ( reg < 10 )
			data &= 0x3F; // clear square duty
	}

	RUN_UNTIL(time);

	if ( addr >= WAVE_RAM )
	{
		wave.write( addr, data );
	}
	else
	{
		int old_data = regs [reg];
		regs [reg] = data;

		if ( addr < VOL_REG)
		{
			// Oscillator
			write_osc( reg / 5, reg, old_data, data );
		}
		else if ( addr == VOL_REG && data != old_data )
		{
			// Master volume
			int i = OSC_COUNT - 1;
			do{
				silence_osc( *oscs [i] );
				--i;
			}while(i >= 0);

			apply_volume();
		}
		else if ( addr == STEREO_REG)
		{
			// Stereo panning
			for ( int i = OSC_COUNT; --i >= 0; )
			{
				Gb_Osc& o = *oscs [i];
				CALC_OUTPUT(i);
				Blip_Buffer* out = o.outputs [bits];
				if ( o.output != out )
				{
					silence_osc( o );
					o.output = out;
				}
			}
		}
		else if ( addr == STATUS_REG && (data ^ old_data) & POWER_MASK)
		{
			// Power control
			frame_phase = 0;
			int i = OSC_COUNT - 1;
			do{
				silence_osc( *oscs [i] );
				--i;
			}while(i >= 0);

			for ( int i = 0; i < 0x20; i++ )
				regs [i] = 0;

			square1.reset();
			square2.reset();
			wave   .reset();
			noise  .reset();

			apply_volume();

			#ifndef USE_GBA_ONLY
			if (wave.mode != MODE_DMG)
			{
			#endif
				square1.length_ctr = 64;
				square2.length_ctr = 64;
				wave   .length_ctr = 256;
				noise  .length_ctr = 64;
			#ifndef USE_GBA_ONLY
			}
			#endif

			regs [STATUS_REG - START_ADDR] = data;
		}
	}
}

void Gb_Apu::apply_stereo()
{
	for ( int i = OSC_COUNT; --i >= 0; )
	{
		Gb_Osc& o = *oscs [i];
		CALC_OUTPUT(i);
		Blip_Buffer* out = o.outputs [bits];
		if ( o.output != out )
		{
			silence_osc( o );
			o.output = out;
		}
	}
}


// Value read back has some bits always set
static uint8_t const masks [] = {
	0x80,0x3F,0x00,0xFF,0xBF,
	0xFF,0x3F,0x00,0xFF,0xBF,
	0x7F,0xFF,0x9F,0xFF,0xBF,
	0xFF,0xFF,0x00,0x00,0xBF,
	0x00,0x00,0x70,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};

int Gb_Apu::read_register( int32_t time, unsigned addr )
{
	RUN_UNTIL(time);

	int reg = addr - START_ADDR;
	if ( (unsigned) reg >= REGISTER_COUNT)
		return 0;

	if ( addr >= WAVE_RAM )
		return wave.read( addr );

	int mask = masks [reg];
	if ( wave.agb_mask && (reg == 10 || reg == 12) )
		mask = 0x1F; // extra implemented bits in wave regs on AGB
	int data = regs [reg] | mask;

	// Status register
	if ( addr == STATUS_REG)
	{
		data &= 0xF0;
		data |= (int) square1.enabled << 0;
		data |= (int) square2.enabled << 1;
		data |= (int) wave   .enabled << 2;
		data |= (int) noise  .enabled << 3;
	}

	return data;
}

//////////////////////////////////////////
// Gb_Apu_State.cpp
//////////////////////////////////////////

#define REFLECT( x, y ) (save ?       (io->y) = (x) :         (x) = (io->y)          )

inline int32_t Gb_Apu::save_load( gb_apu_state_t* io, bool save )
{
	int format = FORMAT0;
	REFLECT( format, format );
	if (format != FORMAT0)
	{
		// Unsupported sound save state format
		return -1;
	}

	int version = 0;
	REFLECT( version, version );

	// Registers and wave RAM
	if ( save )
		__builtin_memcpy( io->regs, regs, sizeof io->regs );
	else
		__builtin_memcpy( regs, io->regs, sizeof     regs );

	// Frame sequencer
	REFLECT( frame_time,  frame_time  );
	REFLECT( frame_phase, frame_phase );

	REFLECT( square1.sweep_freq,    sweep_freq );
	REFLECT( square1.sweep_delay,   sweep_delay );
	REFLECT( square1.sweep_enabled, sweep_enabled );
	REFLECT( square1.sweep_neg,     sweep_neg );

	REFLECT( noise.divider,         noise_divider );
	REFLECT( wave.sample_buf,       wave_buf );

	return 0;
}

// second function to avoid inline limits of some compilers
inline void Gb_Apu::save_load2( gb_apu_state_t* io, bool save )
{
	for ( int i = OSC_COUNT; --i >= 0; )
	{
		Gb_Osc& osc = *oscs [i];
		REFLECT( osc.delay,      delay      [i] );
		REFLECT( osc.length_ctr, length_ctr [i] );
		REFLECT( osc.phase,      phase      [i] );
		REFLECT( osc.enabled,    enabled    [i] );

		if ( i != 2 )
		{
			int j = min( i, 2 );
			Gb_Env& env = STATIC_CAST(Gb_Env&,osc);
			REFLECT( env.env_delay,   env_delay   [j] );
			REFLECT( env.volume,      env_volume  [j] );
			REFLECT( env.env_enabled, env_enabled [j] );
		}
	}
}

void Gb_Apu::save_state( gb_apu_state_t* out )
{
	(void) save_load( out, true );
	save_load2( out, true );
}

int32_t Gb_Apu::load_state( gb_apu_state_t const& in )
{
	int32_t retval = save_load(CONST_CAST(gb_apu_state_t*,&in), false);
	if(retval != 0)
		return retval;
	
	save_load2( CONST_CAST(gb_apu_state_t*,&in), false );

	apply_stereo();
	SYNTH_VOLUME(0);          // suppress output for the moment
	run_until_(last_time);    // get last_amp updated
	apply_volume();             // now use correct volume

	return 0;
}
