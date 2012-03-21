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

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "blargg_source.h"
#include "../System.h"
#include "Gb_Apu.h"

/*============================================================
	GB APU
============================================================ */

#define VOL_REG 0xFF24
#define STEREO_REG 0xFF25
#define STATUS_REG 0xFF26
#define WAVE_RAM 0xFF30
#define POWER_MASK 0x80

#define OSC_COUNT 4

INLINE int Gb_Apu::calc_output( int osc ) const
{
	int bits = regs [STEREO_REG - START_ADDR] >> osc;
	return (bits >> 3 & 2) | (bits & 1);
}

void Gb_Apu::set_output( Blip_Buffer* center, Blip_Buffer* left, Blip_Buffer* right, int osc )
{
	int i;

	i = osc;
	do
	{
		Gb_Osc& o = *oscs [i];
		o.outputs [1] = right;
		o.outputs [2] = left;
		o.outputs [3] = center;
		o.output = o.outputs [calc_output( i )];
		++i;
	}
	while ( i < osc );
}

void Gb_Apu::synth_volume( int iv )
{
	double v = volume_ * 0.60 / OSC_COUNT / 15 /*steps*/ / 8 /*master vol range*/ * iv;
	good_synth.volume( v );
	med_synth .volume( v );
}

void Gb_Apu::apply_volume()
{
	int data, left, right, vol_tmp;
	data  = regs [VOL_REG - START_ADDR];
	left  = data >> 4 & 7;
	right = data & 7;
	vol_tmp = left < right ? right : left;
	synth_volume( vol_tmp + 1 );
}

void Gb_Apu::volume( double v )
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

	/* Click reduction makes DAC off generate same output as volume 0*/
	int dac_off_amp = 0;

	oscs [0]->dac_off_amp = dac_off_amp;
	oscs [1]->dac_off_amp = dac_off_amp;
	oscs [2]->dac_off_amp = dac_off_amp;
	oscs [3]->dac_off_amp = dac_off_amp;

	/* AGB always eliminates clicks on wave channel using same method*/
	wave.dac_off_amp = -DAC_BIAS;
}

void Gb_Apu::reset( uint32_t mode, bool agb_wave )
{
	/* Hardware mode*/
	mode = MODE_AGB; /* using AGB wave features implies AGB hardware*/
	wave.agb_mask = 0xFF;
	oscs [0]->mode = mode;
	oscs [1]->mode = mode;
	oscs [2]->mode = mode;
	oscs [3]->mode = mode;
	reduce_clicks( reduce_clicks_ );

	/* Reset state*/
	frame_time  = 0;
	last_time   = 0;
	frame_phase = 0;

	for ( int i = 0; i < 32; i++ )
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

	/* Load initial wave RAM*/
	static unsigned char const initial_wave [2] [16] = {
		{0x84,0x40,0x43,0xAA,0x2D,0x78,0x92,0x3C,0x60,0x59,0x59,0xB0,0x34,0xB8,0x2E,0xDA},
		{0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF},
	};
	for ( int b = 2; --b >= 0; )
	{
		/* Init both banks (does nothing if not in AGB mode)*/
		write_register( 0, 0xFF1A, b * 0x40 );
		for ( unsigned i = 0; i < sizeof initial_wave [0]; i++ )
			write_register( 0, i + WAVE_RAM, initial_wave [1] [i] );
	}
}

Gb_Apu::Gb_Apu()
{
	int i;

	wave.wave_ram = &regs [WAVE_RAM - START_ADDR];

	oscs [0] = &square1;
	oscs [1] = &square2;
	oscs [2] = &wave;
	oscs [3] = &noise;

	for ( i = OSC_COUNT; --i >= 0; )
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
	frame_period = 4194304 / 512; /* 512 Hz*/

	volume_ = 1.0;
	reset(MODE_AGB, false);
}

void Gb_Apu::run_until_( int32_t end_time )
{
	int32_t time;

	do{
		/* run oscillators*/
		time = end_time;
		if ( time > frame_time )
			time = frame_time;

		square1.run( last_time, time );
		square2.run( last_time, time );
		wave   .run( last_time, time );
		noise  .run( last_time, time );
		last_time = time;

		if ( time == end_time )
			break;

		/* run frame sequencer*/
		frame_time += frame_period * CLK_MUL;
		switch ( frame_phase++ )
		{
			case 2:
			case 6:
				/* 128 Hz*/
				square1.clock_sweep();
			case 0:
			case 4:
				/* 256 Hz*/
				square1.clock_length();
				square2.clock_length();
				wave   .clock_length();
				noise  .clock_length();
				break;

			case 7:
				/* 64 Hz*/
				frame_phase = 0;
				square1.clock_envelope();
				square2.clock_envelope();
				noise  .clock_envelope();
		}
	}while(1);
}

void Gb_Apu::silence_osc( Gb_Osc& o )
{
	int delta;

	delta = -o.last_amp;
	if ( delta )
	{
		o.last_amp = 0;
		if ( o.output )
		{
			med_synth.offset( last_time, delta, o.output );
		}
	}
}

void Gb_Apu::write_register( int32_t time, unsigned addr, int data )
{
	int reg = addr - START_ADDR;
	if ( (unsigned) reg >= REGISTER_COUNT )
		return;

	if ( addr < STATUS_REG && !(regs [STATUS_REG - START_ADDR] & POWER_MASK) )
		return;	/* Power is off*/

	if ( time > last_time )
		run_until_( time );

	if ( addr >= WAVE_RAM )
	{
		wave.write( addr, data );
	}
	else
	{
		int old_data = regs [reg];
		regs [reg] = data;

		if ( addr < VOL_REG )
		{
			/* Oscillator*/
			write_osc( reg / 5, reg, old_data, data );
		}
		else if ( addr == VOL_REG && data != old_data )
		{
			/* Master volume*/
			for ( int i = OSC_COUNT; --i >= 0; )
				silence_osc( *oscs [i] );

			apply_volume();
		}
		else if ( addr == STEREO_REG )
		{
			/* Stereo panning*/
			for ( int i = OSC_COUNT; --i >= 0; )
			{
				Gb_Osc& o = *oscs [i];
				Blip_Buffer* out = o.outputs [calc_output( i )];
				if ( o.output != out )
				{
					silence_osc( o );
					o.output = out;
				}
			} }
		else if ( addr == STATUS_REG && (data ^ old_data) & POWER_MASK )
		{
			/* Power control*/
			frame_phase = 0;
			for ( int i = OSC_COUNT; --i >= 0; )
				silence_osc( *oscs [i] );

			for ( int i = 0; i < 32; i++ )
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

			regs [STATUS_REG - START_ADDR] = data;
		}
	}
}

void Gb_Apu::apply_stereo()
{
	int i;

	for ( i = OSC_COUNT; --i >= 0; )
	{
		Gb_Osc& o = *oscs [i];
		Blip_Buffer* out = o.outputs [calc_output( i )];
		if ( o.output != out )
		{
			silence_osc( o );
			o.output = out;
		}
	}
}


int Gb_Apu::read_register( int32_t time, unsigned addr )
{
	int reg, mask, data;

	if ( time > last_time )
		run_until_( time );

	reg = addr - START_ADDR;
	if ( (unsigned) reg >= REGISTER_COUNT )
		return 0;

	if ( addr >= WAVE_RAM )
		return wave.read( addr );

	/* Value read back has some bits always set*/
	static unsigned char const masks [] = {
		0x80,0x3F,0x00,0xFF,0xBF,
		0xFF,0x3F,0x00,0xFF,0xBF,
		0x7F,0xFF,0x9F,0xFF,0xBF,
		0xFF,0xFF,0x00,0x00,0xBF,
		0x00,0x00,0x70,
		0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
	};

	mask = masks [reg];

	if (reg == 10 || reg == 12)
		mask = 0x1F; /* extra implemented bits in wave regs on AGB*/

	data = regs [reg] | mask;

	/* Status register*/
	if ( addr == STATUS_REG )
	{
		data &= 0xF0;
		data |= (int) square1.enabled << 0;
		data |= (int) square2.enabled << 1;
		data |= (int) wave   .enabled << 2;
		data |= (int) noise  .enabled << 3;
	}

	return data;
}

#define REFLECT( x, y ) (save ?       (io->y) = (x) :         (x) = (io->y)          )

INLINE const char* Gb_Apu::save_load( gb_apu_state_t* io, bool save )
{
	int format, version;

	format = io->format0;

	REFLECT( format, format );
	if ( format != io->format0 )
		return "Unsupported sound save state format";

	version = 0;
	REFLECT( version, version );

	/* Registers and wave RAM*/
	if ( save )
		memcpy( io->regs, regs, sizeof io->regs );
	else
		memcpy( regs, io->regs, sizeof     regs );

	/* Frame sequencer*/
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

/* second function to avoid inline limits of some compilers*/
INLINE void Gb_Apu::save_load2( gb_apu_state_t* io, bool save )
{
	int i;
	for ( i = OSC_COUNT; --i >= 0; )
	{
		Gb_Osc& osc = *oscs [i];
		REFLECT( osc.delay,      delay      [i] );
		REFLECT( osc.length_ctr, length_ctr [i] );
		REFLECT( osc.phase,      phase      [i] );
		REFLECT( osc.enabled,    enabled    [i] );

		if ( i != 2 )
		{
			int j = 2 < i ? 2 : i;
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

const char * Gb_Apu::load_state( gb_apu_state_t const& in )
{
	RETURN_ERR( save_load( CONST_CAST(gb_apu_state_t*,&in), false));
	save_load2( CONST_CAST(gb_apu_state_t*,&in), false );

	apply_stereo();
	synth_volume( 0 );          /* suppress output for the moment*/
	run_until_( last_time );    /* get last_amp updated*/
	apply_volume();             /* now use correct volume*/

	return 0;
}

/*============================================================
	GB OSCS
============================================================ */

#define TRIGGER_MASK 0x80
#define LENGTH_ENABLED 0x40

#define VOLUME_SHIFT_PLUS_FOUR	6
#define SIZE20_MASK 0x20

void Gb_Osc::reset()
{
        output   = 0;
        last_amp = 0;
        delay    = 0;
        phase    = 0;
        enabled  = false;
}

INLINE void Gb_Osc::update_amp( int32_t time, int new_amp )
{
	int delta = new_amp - last_amp;
        if ( delta )
        {
                last_amp = new_amp;
                med_synth->offset( time, delta, output );
        }
}

void Gb_Osc::clock_length()
{
        if ( (regs [4] & LENGTH_ENABLED) && length_ctr )
        {
                if ( --length_ctr <= 0 )
                        enabled = false;
        }
}

INLINE int Gb_Env::reload_env_timer()
{
        int raw = regs [2] & 7;
        env_delay = (raw ? raw : 8);
        return raw;
}

void Gb_Env::clock_envelope()
{
        if ( env_enabled && --env_delay <= 0 && reload_env_timer() )
        {
                int v = volume + (regs [2] & 0x08 ? +1 : -1);
                if ( 0 <= v && v <= 15 )
                        volume = v;
                else
                        env_enabled = false;
        }
}

#define reload_sweep_timer() \
        sweep_delay = (regs [0] & PERIOD_MASK) >> 4; \
        if ( !sweep_delay ) \
                sweep_delay = 8;

void Gb_Sweep_Square::calc_sweep( bool update )
{
	int shift, delta, freq;

        shift = regs [0] & SHIFT_MASK;
        delta = sweep_freq >> shift;
        sweep_neg = (regs [0] & 0x08) != 0;
        freq = sweep_freq + (sweep_neg ? -delta : delta);

        if ( freq > 0x7FF )
                enabled = false;
        else if ( shift && update )
        {
                sweep_freq = freq;

                regs [3] = freq & 0xFF;
                regs [4] = (regs [4] & ~0x07) | (freq >> 8 & 0x07);
        }
}

void Gb_Sweep_Square::clock_sweep()
{
        if ( --sweep_delay <= 0 )
        {
                reload_sweep_timer();
                if ( sweep_enabled && (regs [0] & PERIOD_MASK) )
                {
                        calc_sweep( true  );
                        calc_sweep( false );
                }
        }
}

int Gb_Wave::access( unsigned addr ) const
{
	addr = (phase & BANK_SIZE_MIN_ONE) >> 1;
	return addr & 0x0F;
}

// write_register

int Gb_Osc::write_trig( int frame_phase, int max_len, int old_data )
{
        int data = regs [4];

        if ( (frame_phase & 1) && !(old_data & LENGTH_ENABLED) && length_ctr )
        {
                if ( (data & LENGTH_ENABLED))
                        length_ctr--;
        }

        if ( data & TRIGGER_MASK )
        {
                enabled = true;
                if ( !length_ctr )
                {
                        length_ctr = max_len;
                        if ( (frame_phase & 1) && (data & LENGTH_ENABLED) )
                                length_ctr--;
                }
        }

        if ( !length_ctr )
                enabled = false;

        return data & TRIGGER_MASK;
}

INLINE void Gb_Env::zombie_volume( int old, int data )
{
	int v = volume;

	// CGB-05 behavior, very close to AGB behavior as well
	if ( (old ^ data) & 8 )
	{
		if ( !(old & 8) )
		{
			v++;
			if ( old & 7 )
				v++;
		}

		v = 16 - v;
	}
	else if ( (old & 0x0F) == 8 )
		v++;
	volume = v & 0x0F;
}

bool Gb_Env::write_register( int frame_phase, int reg, int old, int data )
{
        int const max_len = 64;

        switch ( reg )
	{
		case 1:
			length_ctr = max_len - (data & (max_len - 1));
			break;

		case 2:
			if ( !GB_ENV_DAC_ENABLED() )
				enabled = false;

			zombie_volume( old, data );

			if ( (data & 7) && env_delay == 8 )
			{
				env_delay = 1;
				clock_envelope(); // TODO: really happens at next length clock
			}
			break;

		case 4:
			if ( write_trig( frame_phase, max_len, old ) )
			{
				volume = regs [2] >> 4;
				reload_env_timer();
				env_enabled = true;
				if ( frame_phase == 7 )
					env_delay++;
				if ( !GB_ENV_DAC_ENABLED() )
					enabled = false;
				return true;
			}
	}
        return false;
}

bool Gb_Square::write_register( int frame_phase, int reg, int old_data, int data )
{
        bool result = Gb_Env::write_register( frame_phase, reg, old_data, data );
        if ( result )
                delay = (delay & (CLK_MUL_MUL_4 - 1)) + period();
        return result;
}

INLINE void Gb_Noise::write_register( int frame_phase, int reg, int old_data, int data )
{
        if ( Gb_Env::write_register( frame_phase, reg, old_data, data ) )
        {
                phase = 0x7FFF;
                delay += CLK_MUL_MUL_8;
        }
}

INLINE void Gb_Sweep_Square::write_register( int frame_phase, int reg, int old_data, int data )
{
        if ( reg == 0 && sweep_enabled && sweep_neg && !(data & 0x08) )
                enabled = false; // sweep negate disabled after used

        if ( Gb_Square::write_register( frame_phase, reg, old_data, data ) )
        {
                sweep_freq = GB_OSC_FREQUENCY();
                sweep_neg = false;
                reload_sweep_timer();
                sweep_enabled = (regs [0] & (PERIOD_MASK | SHIFT_MASK)) != 0;
                if ( regs [0] & SHIFT_MASK )
                        calc_sweep( false );
        }
}

void Gb_Wave::corrupt_wave()
{
        int pos = ((phase + 1) & BANK_SIZE_MIN_ONE) >> 1;
        if ( pos < 4 )
                wave_ram [0] = wave_ram [pos];
        else
                for ( int i = 4; --i >= 0; )
                        wave_ram [i] = wave_ram [(pos & ~3) + i];
}

INLINE void Gb_Wave::write_register( int frame_phase, int reg, int old_data, int data )
{
        switch ( reg )
	{
		case 0:
			if ( !GB_WAVE_DAC_ENABLED() )
				enabled = false;
			break;

		case 1:
			length_ctr = 256 - data;
			break;

		case 4:
			bool was_enabled = enabled;
			if ( write_trig( frame_phase, 256, old_data ) )
			{
				if ( !GB_WAVE_DAC_ENABLED() )
					enabled = false;
				phase = 0;
				delay    = period() + CLK_MUL_MUL_6;
			}
	}
}

void Gb_Apu::write_osc( int index, int reg, int old_data, int data )
{
        reg -= index * 5;
        switch ( index )
	{
		case 0:
			square1.write_register( frame_phase, reg, old_data, data );
			break;
		case 1:
			square2.write_register( frame_phase, reg, old_data, data );
			break;
		case 2:
			wave   .write_register( frame_phase, reg, old_data, data );
			break;
		case 3:
			noise  .write_register( frame_phase, reg, old_data, data );
			break;
	}
}

/* Synthesis*/

void Gb_Square::run( int32_t time, int32_t end_time )
{
        /* Calc duty and phase*/
        static unsigned char const duty_offsets [4] = { 1, 1, 3, 7 };
        static unsigned char const duties       [4] = { 1, 2, 4, 6 };
        int const duty_code = regs [1] >> 6;
        int32_t duty_offset = duty_offsets [duty_code];
        int32_t duty = duties [duty_code];
	/* AGB uses inverted duty*/
	duty_offset -= duty;
	duty = 8 - duty;
        int ph = (phase + duty_offset) & 7;

        /* Determine what will be generated*/
        int vol = 0;
        Blip_Buffer* const out = output;
        if ( out )
        {
                int amp = dac_off_amp;
                if ( GB_ENV_DAC_ENABLED() )
                {
                        if ( enabled )
                                vol = volume;

			amp = -(vol >> 1);

                        /* Play inaudible frequencies as constant amplitude*/
                        if ( GB_OSC_FREQUENCY() >= 0x7FA && delay < CLK_MUL_MUL_32 )
                        {
                                amp += (vol * duty) >> 3;
                                vol = 0;
                        }

                        if ( ph < duty )
                        {
                                amp += vol;
                                vol = -vol;
                        }
                }
                update_amp( time, amp );
        }

        /* Generate wave*/
        time += delay;
        if ( time < end_time )
        {
                int const per = period();
                if ( !vol )
                {
                        /* Maintain phase when not playing*/
                        int count = (end_time - time + per - 1) / per;
                        ph += count; /* will be masked below*/
                        time += (int32_t) count * per;
                }
                else
                {
                        /* Output amplitude transitions*/
                        int delta = vol;
                        do
                        {
                                ph = (ph + 1) & 7;
                                if ( ph == 0 || ph == duty )
                                {
                                        good_synth->offset_inline( time, delta, out );
                                        delta = -delta;
                                }
                                time += per;
                        }
                        while ( time < end_time );

                        if ( delta != vol )
                                last_amp -= delta;
                }
                phase = (ph - duty_offset) & 7;
        }
        delay = time - end_time;
}

/* Quickly runs LFSR for a large number of clocks. For use when noise is generating*/
/* no sound.*/
static unsigned run_lfsr( unsigned s, unsigned mask, int count )
{
	/* optimization used in several places:*/
	/* ((s & (1 << b)) << n) ^ ((s & (1 << b)) << (n + 1)) = (s & (1 << b)) * (3 << n)*/

	if ( mask == 0x4000 )
	{
		if ( count >= 32767 )
			count %= 32767;

		/* Convert from Fibonacci to Galois configuration,*/
		/* shifted left 1 bit*/
		s ^= (s & 1) * 0x8000;

		/* Each iteration is equivalent to clocking LFSR 255 times*/
		while ( (count -= 255) > 0 )
			s ^= ((s & 0xE) << 12) ^ ((s & 0xE) << 11) ^ (s >> 3);
		count += 255;

		/* Each iteration is equivalent to clocking LFSR 15 times*/
		/* (interesting similarity to single clocking below)*/
		while ( (count -= 15) > 0 )
			s ^= ((s & 2) * (3 << 13)) ^ (s >> 1);
		count += 15;

		/* Remaining singles*/
		do{
			--count;
			s = ((s & 2) * (3 << 13)) ^ (s >> 1);
		}while(count >= 0);

		/* Convert back to Fibonacci configuration*/
		s &= 0x7FFF;
	}
	else if ( count < 8)
	{
		/* won't fully replace upper 8 bits, so have to do the unoptimized way*/
		do{
			--count;
			s = (s >> 1 | mask) ^ (mask & -((s - 1) & 2));
		}while(count >= 0);
	}
	else
	{
		if ( count > 127 )
		{
			count %= 127;
			if ( !count )
				count = 127; /* must run at least once*/
		}

		/* Need to keep one extra bit of history*/
		s = s << 1 & 0xFF;

		/* Convert from Fibonacci to Galois configuration,*/
		/* shifted left 2 bits*/
		s ^= (s & 2) << 7;

		/* Each iteration is equivalent to clocking LFSR 7 times*/
		/* (interesting similarity to single clocking below)*/
		while ( (count -= 7) > 0 )
			s ^= ((s & 4) * (3 << 5)) ^ (s >> 1);
		count += 7;

		/* Remaining singles*/
		while ( --count >= 0 )
			s = ((s & 4) * (3 << 5)) ^ (s >> 1);

		/* Convert back to Fibonacci configuration and*/
		/* repeat last 8 bits above significant 7*/
		s = (s << 7 & 0x7F80) | (s >> 1 & 0x7F);
	}

	return s;
}

void Gb_Noise::run( int32_t time, int32_t end_time )
{
        /* Determine what will be generated*/
        int vol = 0;
        Blip_Buffer* const out = output;
        if ( out )
        {
                int amp = dac_off_amp;
                if ( GB_ENV_DAC_ENABLED() )
                {
                        if ( enabled )
                                vol = volume;

			amp = -(vol >> 1);

                        if ( !(phase & 1) )
                        {
                                amp += vol;
                                vol = -vol;
                        }
                }

                /* AGB negates final output*/
		vol = -vol;
		amp    = -amp;

                update_amp( time, amp );
        }

        /* Run timer and calculate time of next LFSR clock*/
        static unsigned char const period1s [8] = { 1, 2, 4, 6, 8, 10, 12, 14 };
        int const period1 = period1s [regs [3] & 7] * CLK_MUL;
        {
                int extra = (end_time - time) - delay;
                int const per2 = GB_NOISE_PERIOD2(8);
                time += delay + ((divider ^ (per2 >> 1)) & (per2 - 1)) * period1;

                int count = (extra < 0 ? 0 : (extra + period1 - 1) / period1);
                divider = (divider - count) & PERIOD2_MASK;
                delay = count * period1 - extra;
        }

        /* Generate wave*/
        if ( time < end_time )
        {
                unsigned const mask = GB_NOISE_LFSR_MASK();
                unsigned bits = phase;

                int per = GB_NOISE_PERIOD2( period1 * 8 );
                if ( GB_NOISE_PERIOD2_INDEX() >= 0xE )
                {
                        time = end_time;
                }
                else if ( !vol )
                {
                        /* Maintain phase when not playing*/
                        int count = (end_time - time + per - 1) / per;
                        time += (int32_t) count * per;
                        bits = run_lfsr( bits, ~mask, count );
                }
                else
                {
                        /* Output amplitude transitions*/
                        int delta = -vol;
                        do
                        {
                                unsigned changed = bits + 1;
                                bits = bits >> 1 & mask;
                                if ( changed & 2 )
                                {
                                        bits |= ~mask;
                                        delta = -delta;
                                        med_synth->offset_inline( time, delta, out );
                                }
                                time += per;
                        }
                        while ( time < end_time );

                        if ( delta == vol )
                                last_amp += delta;
                }
                phase = bits;
        }
}

void Gb_Wave::run( int32_t time, int32_t end_time )
{
        /* Calc volume*/
        static unsigned char const volumes [8] = { 0, 4, 2, 1, 3, 3, 3, 3 };
        int const volume_idx = regs [2] >> 5 & (agb_mask | 3); /* 2 bits on DMG/CGB, 3 on AGB*/
        int const volume_mul = volumes [volume_idx];

        /* Determine what will be generated*/
        int playing = false;
        Blip_Buffer* const out = output;
        if ( out )
        {
                int amp = dac_off_amp;
                if ( GB_WAVE_DAC_ENABLED() )
                {
                        /* Play inaudible frequencies as constant amplitude*/
                        amp = 128; /* really depends on average of all samples in wave*/

                        /* if delay is larger, constant amplitude won't start yet*/
                        if ( GB_OSC_FREQUENCY() <= 0x7FB || delay > CLK_MUL_MUL_15 )
                        {
                                if ( volume_mul )
                                        playing = (int) enabled;

                                amp = (sample_buf << (phase << 2 & 4) & 0xF0) * playing;
                        }

                        amp = ((amp * volume_mul) >> VOLUME_SHIFT_PLUS_FOUR) - DAC_BIAS;
                }
                update_amp( time, amp );
        }

        /* Generate wave*/
        time += delay;
        if ( time < end_time )
        {
                unsigned char const* wave = wave_ram;

                /* wave size and bank*/
                int const flags = regs [0] & agb_mask;
                int const wave_mask = (flags & SIZE20_MASK) | 0x1F;
                int swap_banks = 0;
                if ( flags & BANK40_MASK)
                {
                        swap_banks = flags & SIZE20_MASK;
                        wave += BANK_SIZE_DIV_TWO - (swap_banks >> 1);
                }

                int ph = phase ^ swap_banks;
                ph = (ph + 1) & wave_mask; /* pre-advance*/

                int const per = period();
                if ( !playing )
                {
                        /* Maintain phase when not playing*/
                        int count = (end_time - time + per - 1) / per;
                        ph += count; /* will be masked below*/
                        time += (int32_t) count * per;
                }
                else
                {
                        /* Output amplitude transitions*/
                        int lamp = last_amp + DAC_BIAS;
                        do
                        {
                                /* Extract nybble*/
                                int nybble = wave [ph >> 1] << (ph << 2 & 4) & 0xF0;
                                ph = (ph + 1) & wave_mask;

                                /* Scale by volume*/
                                int amp = (nybble * volume_mul) >> VOLUME_SHIFT_PLUS_FOUR;

                                int delta = amp - lamp;
                                if ( delta )
                                {
                                        lamp = amp;
                                        med_synth->offset_inline( time, delta, out );
                                }
                                time += per;
                        }
                        while ( time < end_time );
                        last_amp = lamp - DAC_BIAS;
                }
                ph = (ph - 1) & wave_mask; /* undo pre-advance and mask position*/

                /* Keep track of last byte read*/
                if ( enabled )
                        sample_buf = wave [ph >> 1];

                phase = ph ^ swap_banks; /* undo swapped banks*/
        }
        delay = time - end_time;
}

/*============================================================
	BLIP BUFFER
============================================================ */
#include "Blip_Buffer.h"

/* Blip_Buffer 0.4.1. http://www.slack.net/~ant */

#define FIXED_SHIFT 12
#define SAL_FIXED_SHIFT 4096
#define TO_FIXED( f )   int ((f) * SAL_FIXED_SHIFT)
#define FROM_FIXED( f ) ((f) >> FIXED_SHIFT)

Blip_Buffer::Blip_Buffer()
{
        factor_       = INT_MAX;
        buffer_       = 0;
        buffer_size_  = 0;
        sample_rate_  = 0;
        clock_rate_   = 0;
        length_       = 0;

        clear();
}

Blip_Buffer::~Blip_Buffer()
{
	free( buffer_ );
}

void Blip_Buffer::clear( void)
{
        offset_       = 0;
        reader_accum_ = 0;
        if ( buffer_ )
                memset( buffer_, 0, (buffer_size_ + BLIP_BUFFER_EXTRA_) * sizeof (int32_t) );
}

const char * Blip_Buffer::set_sample_rate( long new_rate, int msec )
{
        /* start with maximum length that resampled time can represent*/
        long new_size = (ULONG_MAX >> BLIP_BUFFER_ACCURACY) - BLIP_BUFFER_EXTRA_ - 64;
        if ( msec != 0)
        {
                long s = (new_rate * (msec + 1) + 999) / 1000;
                if ( s < new_size )
                        new_size = s;
        }

        if ( buffer_size_ != new_size )
        {
                void* p = realloc( buffer_, (new_size + BLIP_BUFFER_EXTRA_) * sizeof *buffer_ );
                if ( !p )
                        return "Out of memory";
                buffer_ = (int32_t *) p;
        }

        buffer_size_ = new_size;

        /* update things based on the sample rate*/
        sample_rate_ = new_rate;
        length_ = new_size * 1000 / new_rate - 1;

        /* update these since they depend on sample rate*/
        if ( clock_rate_ )
                factor_ = clock_rate_factor( clock_rate_);

        clear();

        return 0;
}

/* Sets number of source time units per second */

uint32_t Blip_Buffer::clock_rate_factor( long rate ) const
{
        double ratio = (double) sample_rate_ / rate;
        int32_t factor = (int32_t) floor( ratio * (1L << BLIP_BUFFER_ACCURACY) + 0.5 );
        return (uint32_t) factor;
}


/* Blip_Synth */

Blip_Synth::Blip_Synth()
{
        buf          = 0;
        delta_factor = 0;
}

void Blip_Buffer::save_state( blip_buffer_state_t* out )
{
        out->offset_       = offset_;
        out->reader_accum_ = reader_accum_;
        memcpy( out->buf, &buffer_ [offset_ >> BLIP_BUFFER_ACCURACY], sizeof out->buf );
}

void Blip_Buffer::load_state( blip_buffer_state_t const& in )
{
        clear();

        offset_       = in.offset_;
        reader_accum_ = in.reader_accum_;
        memcpy( buffer_, in.buf, sizeof(in.buf));
}
