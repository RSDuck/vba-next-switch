/* Gb_Snd_Emu 0.2.0. http://www.slack.net/~ant/ */
/* Blip_Buffer 0.4.1. http://www.slack.net/~ant */

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

#include "../System.h"

#include "Gb_Apu.h"
#include "blargg_source.h"

unsigned const vol_reg    = 0xFF24;
unsigned const stereo_reg = 0xFF25;
unsigned const status_reg = 0xFF26;
unsigned const wave_ram   = 0xFF30;

int const power_mask = 0x80;

#define osc_count 4

void Gb_Apu::set_output( Blip_Buffer* center, Blip_Buffer* left, Blip_Buffer* right, int osc )
{
	int i;

	i = osc;
	do
	{
		int bits;
		Gb_Osc& o = *oscs [i];
		o.outputs [1] = right;
		o.outputs [2] = left;
		o.outputs [3] = center;
		bits = regs [stereo_reg - start_addr] >> i;
		o.output = o.outputs [((bits >> 3 & 2) | (bits & 1))];
		++i;
	}
	while ( i < osc );
}

void Gb_Apu::synth_volume( int iv )
{
	double v = volume_ * 0.60 / osc_count / 15 /*steps*/ / 8 /*master vol range*/ * iv;
	good_synth.volume( v );
	med_synth .volume( v );
}

void Gb_Apu::apply_volume()
{
	int data, left, right;

	/* TODO: Doesn't handle differing left and right volumes (panning).*/
	/* Not worth the complexity.*/
	data  = regs [vol_reg - start_addr];
	left  = data >> 4 & 7;
	right = data & 7;
	synth_volume( max( left, right ) + 1 );
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
	if ( reduce && wave.mode != mode_agb ) /* AGB already eliminates clicks*/
		dac_off_amp = -dac_bias;

	oscs [0]->dac_off_amp = dac_off_amp;
	oscs [1]->dac_off_amp = dac_off_amp;
	oscs [2]->dac_off_amp = dac_off_amp;
	oscs [3]->dac_off_amp = dac_off_amp;

	/* AGB always eliminates clicks on wave channel using same method*/
	if ( wave.mode == mode_agb )
		wave.dac_off_amp = -dac_bias;
}

void Gb_Apu::reset( uint32_t mode, bool agb_wave )
{
	int i, b;
	static unsigned char const initial_wave [2] [16] = {
		{0x84,0x40,0x43,0xAA,0x2D,0x78,0x92,0x3C,0x60,0x59,0x59,0xB0,0x34,0xB8,0x2E,0xDA},
		{0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF},
	};

	/* Hardware mode*/
	if ( agb_wave )
		mode = mode_agb; /* using AGB wave features implies AGB hardware*/
	wave.agb_mask = agb_wave ? 0xFF : 0;

	oscs [0]->mode = mode;
	oscs [1]->mode = mode;
	oscs [2]->mode = mode;
	oscs [3]->mode = mode;

	reduce_clicks( reduce_clicks_ );

	/* Reset state*/
	frame_time  = 0;
	last_time   = 0;
	frame_phase = 0;

	for ( i = 0; i < 0x20; i++ )
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
	for ( b = 2; --b >= 0; )
	{
		/* Init both banks (does nothing if not in AGB mode)*/
		/* TODO: verify that this works*/
		write_register( 0, 0xFF1A, b * 0x40 );
		for ( i = 0; i < sizeof(initial_wave [0]); i++ )
			write_register( 0, i + wave_ram, initial_wave [(mode != mode_dmg)] [i] );
	}
}

Gb_Apu::Gb_Apu()
{
	int i;

	wave.wave_ram = &regs [wave_ram - start_addr];

	oscs [0] = &square1;
	oscs [1] = &square2;
	oscs [2] = &wave;
	oscs [3] = &noise;

	for ( i = osc_count; --i >= 0; )
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
	/*begin set Tempo ( 1.0)*/
	frame_period = 4194304 / 512; /* 512 Hz*/
	/*end set Tempo ( 1.0)*/

	volume_ = 1.0;
	reset();
}

void Gb_Apu::run_until_( int32_t end_time )
{
	int32_t time;

	do
	{
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
		frame_time += frame_period * clk_mul;
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

void Gb_Apu::end_frame( int32_t end_time )
{
	if ( end_time > last_time )
		run_until_( end_time );

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
			o.output->set_modified();
			med_synth.offset( last_time, delta, o.output );
		}
	}
}

void Gb_Apu::write_register( int32_t time, unsigned addr, int data )
{
	int reg = addr - start_addr;
	if ( (unsigned) reg >= register_count )
		return;

	if ( addr < status_reg && !(regs [status_reg - start_addr] & power_mask) )
	{
		/* Power is off*/

		/* length counters can only be written in DMG mode*/
		if ( wave.mode != mode_dmg || (reg != 1 && reg != 5+1 && reg != 10+1 && reg != 15+1) )
			return;

		if ( reg < 10 )
			data &= 0x3F; /* clear square duty*/
	}

	if ( time > last_time )
		run_until_( time );

	if ( addr >= wave_ram )
	{
		wave.write( addr, data );
	}
	else
	{
		int old_data = regs [reg];
		regs [reg] = data;

		if ( addr < vol_reg )
		{
			/* Oscillator*/
			write_osc( reg / 5, reg, old_data, data );
		}
		else if ( addr == vol_reg && data != old_data )
		{
			/* Master volume*/
			for ( int i = osc_count; --i >= 0; )
				silence_osc( *oscs [i] );

			apply_volume();
		}
		else if ( addr == stereo_reg )
		{
			/* Stereo panning*/
			for ( int i = osc_count; --i >= 0; )
			{
				int bits;
				Gb_Osc& o = *oscs [i];
				 bits = regs [stereo_reg - start_addr] >> i;
				Blip_Buffer* out = o.outputs [((bits >> 3 & 2) | (bits & 1))];
				if ( o.output != out )
				{
					silence_osc( o );
					o.output = out;
				}
			}
		}
		else if ( addr == status_reg && (data ^ old_data) & power_mask )
		{
			/* Power control*/
			frame_phase = 0;
			for ( int i = osc_count; --i >= 0; )
				silence_osc( *oscs [i] );

			for ( int i = 0; i < 0x20; i++ )
				regs [i] = 0;

			square1.reset();
			square2.reset();
			wave   .reset();
			noise  .reset();

			apply_volume();

			if ( wave.mode != mode_dmg )
			{
				square1.length_ctr = 64;
				square2.length_ctr = 64;
				wave   .length_ctr = 256;
				noise  .length_ctr = 64;
			}

			regs [status_reg - start_addr] = data;
		}
	}
}

void Gb_Apu::apply_stereo()
{
	for ( int i = osc_count; --i >= 0; )
	{
		int bits;
		Gb_Osc& o = *oscs [i];
		bits = regs [stereo_reg - start_addr] >> i;
		Blip_Buffer* out = o.outputs [((bits >> 3 & 2) | (bits & 1))];
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

	reg = addr - start_addr;
	if ( (unsigned) reg >= register_count )
		return 0;

	if ( addr >= wave_ram )
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

	if ( wave.agb_mask && (reg == 10 || reg == 12) )
		mask = 0x1F; /* extra implemented bits in wave regs on AGB*/

	data = regs [reg] | mask;

	/* Status register*/
	if ( addr == status_reg )
	{
		data &= 0xF0;
		data |= (int) square1.enabled << 0;
		data |= (int) square2.enabled << 1;
		data |= (int) wave   .enabled << 2;
		data |= (int) noise  .enabled << 3;
	}

	return data;
}

/* Gb_Apu_State.cpp*/

#define REFLECT( x, y ) (save ?       (io->y) = (x) :         (x) = (io->y)          )

INLINE const char* Gb_Apu::save_load( gb_apu_state_t* io, bool save )
{
	int format = io->format0;
	REFLECT( format, format );
	if ( format != io->format0 )
		return "Unsupported sound save state format";

	int version = 0;
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
	for ( int i = osc_count; --i >= 0; )
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

bool const cgb_02 = false; /* enables bug in early CGB units that causes problems in some games*/
bool const cgb_05 = false; /* enables CGB-05 zombie behavior*/

#define trigger_mask 0x80
#define length_enabled 0x40

void Gb_Osc::reset()
{
        output   = 0;
        last_amp = 0;
        delay    = 0;
        phase    = 0;
        enabled  = false;
}

/* Units*/

void Gb_Osc::clock_length()
{
        if ( (regs [4] & length_enabled) && length_ctr )
        {
                if ( --length_ctr <= 0 )
                        enabled = false;
        }
}

void Gb_Env::clock_envelope()
{
	int raw;

	raw = regs[2] & 7;
	env_delay = (raw ? raw : 8);

        if ( env_enabled && --env_delay <= 0 && raw )
        {
                int v = volume + (regs [2] & 0x08 ? +1 : -1);
                if ( 0 <= v && v <= 15 )
                        volume = v;
                else
                        env_enabled = false;
        }
}

#define reload_sweep_timer() \
        sweep_delay = (regs [0] & period_mask) >> 4; \
        if ( !sweep_delay ) \
                sweep_delay = 8;

void Gb_Sweep_Square::calc_sweep( bool update )
{
        int const shift = regs [0] & shift_mask;
        int const delta = sweep_freq >> shift;
        sweep_neg = (regs [0] & 0x08) != 0;
        int const freq = sweep_freq + (sweep_neg ? -delta : delta);

        if ( freq > 0x7FF )
        {
                enabled = false;
        }
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
                if ( sweep_enabled && (regs [0] & period_mask) )
                {
                        calc_sweep( true  );
                        calc_sweep( false );
                }
        }
}

int Gb_Wave::access( unsigned addr ) const
{
	addr = phase & (bank_size - 1);
	if ( mode == mode_dmg )
	{
		addr++;
		if ( delay > clk_mul )
			return -1; /* can only access within narrow time window while playing*/
	}

	addr >>= 1;
	return addr & 0x0F;
}

/* write_register*/

int Gb_Osc::write_trig( int frame_phase, int max_len, int old_data )
{
        int data = regs [4];

        if ( (frame_phase & 1) && !(old_data & length_enabled) && length_ctr )
        {
                if ( (data & length_enabled) || cgb_02 )
                        length_ctr--;
        }

        if ( data & trigger_mask )
        {
                enabled = true;
                if ( !length_ctr )
                {
                        length_ctr = max_len;
                        if ( (frame_phase & 1) && (data & length_enabled) )
                                length_ctr--;
                }
        }

        if ( !length_ctr )
                enabled = false;

        return data & trigger_mask;
}

INLINE void Gb_Env::zombie_volume( int old, int data )
{
        int v = volume;
        if ( mode == mode_agb || cgb_05 )
        {
                /* CGB-05 behavior, very close to AGB behavior as well*/
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
                {
                        v++;
                }
        }
        else
        {
                /* CGB-04&02 behavior, very close to MGB behavior as well*/
                if ( !(old & 7) && env_enabled )
                        v++;
                else if ( !(old & 8) )
                        v += 2;

                if ( (old ^ data) & 8 )
                        v = 16 - v;
        }
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
			if ( !dac_enabled() )
				enabled = false;

			zombie_volume( old, data );

			if ( (data & 7) && env_delay == 8 )
			{
				env_delay = 1;
				clock_envelope(); /* TODO: really happens at next length clock*/
			}
			break;

		case 4:
			if ( write_trig( frame_phase, max_len, old ) )
			{
				int raw;

				volume = regs [2] >> 4;
				raw = regs [2] & 7;
				env_delay = (raw ? raw : 8);
				env_enabled = true;
				if ( frame_phase == 7 )
					env_delay++;
				if ( !dac_enabled() )
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
                delay = (delay & (4 * clk_mul - 1)) + period();
        return result;
}

INLINE void Gb_Noise::write_register( int frame_phase, int reg, int old_data, int data )
{
        if ( Gb_Env::write_register( frame_phase, reg, old_data, data ) )
        {
                phase = 0x7FFF;
                delay += 8 * clk_mul;
        }
}

INLINE void Gb_Sweep_Square::write_register( int frame_phase, int reg, int old_data, int data )
{
        if ( reg == 0 && sweep_enabled && sweep_neg && !(data & 0x08) )
                enabled = false; /* sweep negate disabled after used*/

        if ( Gb_Square::write_register( frame_phase, reg, old_data, data ) )
        {
                sweep_freq = frequency();
                sweep_neg = false;
                reload_sweep_timer();
                sweep_enabled = (regs [0] & (period_mask | shift_mask)) != 0;
                if ( regs [0] & shift_mask )
                        calc_sweep( false );
        }
}

void Gb_Wave::corrupt_wave()
{
        int pos = ((phase + 1) & (bank_size - 1)) >> 1;
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
			if ( !dac_enabled() )
				enabled = false;
			break;

		case 1:
			length_ctr = 256 - data;
			break;

		case 4:
			bool was_enabled = enabled;
			if ( write_trig( frame_phase, 256, old_data ) )
			{
				if ( !dac_enabled() )
					enabled = false;
				else if ( mode == mode_dmg && was_enabled && (unsigned) (delay - 2 * clk_mul) < 2 * clk_mul )
					corrupt_wave();
				phase = 0;
				delay    = period() + 6 * clk_mul;
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
        if ( mode == mode_agb )
        {
                /* AGB uses inverted duty*/
                duty_offset -= duty;
                duty = 8 - duty;
        }
        int ph = (phase + duty_offset) & 7;

        /* Determine what will be generated*/
        int vol = 0;
        Blip_Buffer* const out = output;
        if ( out )
        {
                int amp = dac_off_amp;
                if ( dac_enabled() )
                {
                        if ( enabled )
                                vol = volume;

                        amp = -dac_bias;
                        if ( mode == mode_agb )
                                amp = -(vol >> 1);

                        /* Play inaudible frequencies as constant amplitude*/
                        if ( frequency() >= 0x7FA && delay < 32 * clk_mul )
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
		output->set_modified();
		int delta = amp - last_amp;
		if ( delta )
		{
			last_amp = amp;
			med_synth->offset( time, delta, output );
		}
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
		s ^= (s & 2) * 0x80;

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
                if ( dac_enabled() )
                {
                        if ( enabled )
                                vol = volume;

                        amp = -dac_bias;
                        if ( mode == mode_agb )
                                amp = -(vol >> 1);

                        if ( !(phase & 1) )
                        {
                                amp += vol;
                                vol = -vol;
                        }
                }

                /* AGB negates final output*/
                if ( mode == mode_agb )
                {
                        vol = -vol;
                        amp    = -amp;
                }

		output->set_modified();
		int delta = amp - last_amp;
		if ( delta )
		{
			last_amp = amp;
			med_synth->offset( time, delta, output );
		}
        }

        /* Run timer and calculate time of next LFSR clock*/
        static unsigned char const period1s [8] = { 1, 2, 4, 6, 8, 10, 12, 14 };
        int const period1 = period1s [regs [3] & 7] * clk_mul;
        {
                int extra = (end_time - time) - delay;
                int const per2 = period2();
                time += delay + ((divider ^ (per2 >> 1)) & (per2 - 1)) * period1;

                int count = (extra < 0 ? 0 : (extra + period1 - 1) / period1);
                divider = (divider - count) & period2_mask;
                delay = count * period1 - extra;
        }

        /* Generate wave*/
        if ( time < end_time )
        {
                unsigned const mask = lfsr_mask();
                unsigned bits = phase;

                int per = period2( period1 * 8 );
                if ( period2_index() >= 0xE )
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

#define volume_shift	2
#define volume_shift_plus_four	6
#define size20_mask 0x20

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
                if ( dac_enabled() )
                {
                        /* Play inaudible frequencies as constant amplitude*/
                        amp = 8 << 4; /* really depends on average of all samples in wave*/

                        /* if delay is larger, constant amplitude won't start yet*/
                        if ( frequency() <= 0x7FB || delay > 15 * clk_mul )
                        {
                                if ( volume_mul )
                                        playing = (int) enabled;

                                amp = (sample_buf << (phase << 2 & 4) & 0xF0) * playing;
                        }

                        amp = ((amp * volume_mul) >> (volume_shift_plus_four)) - dac_bias;
                }
		output->set_modified();
		int delta = amp - last_amp;
		if ( delta )
		{
			last_amp = amp;
			med_synth->offset( time, delta, output );
		}
        }

        /* Generate wave*/
        time += delay;
        if ( time < end_time )
        {
                unsigned char const* wave = wave_ram;

                /* wave size and bank*/
                int const flags = regs [0] & agb_mask;
                int const wave_mask = (flags & size20_mask) | 0x1F;
                int swap_banks = 0;
                if ( flags & bank40_mask )
                {
                        swap_banks = flags & size20_mask;
                        wave += bank_size/2 - (swap_banks >> 1);
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
                        int lamp = last_amp + dac_bias;
                        do
                        {
                                /* Extract nybble*/
                                int nybble = wave [ph >> 1] << (ph << 2 & 4) & 0xF0;
                                ph = (ph + 1) & wave_mask;

                                /* Scale by volume*/
                                int amp = (nybble * volume_mul) >> (volume_shift_plus_four);

                                int delta = amp - lamp;
                                if ( delta )
                                {
                                        lamp = amp;
                                        med_synth->offset_inline( time, delta, out );
                                }
                                time += per;
                        }
                        while ( time < end_time );
                        last_amp = lamp - dac_bias;
                }
                ph = (ph - 1) & wave_mask; /* undo pre-advance and mask position*/

                /* Keep track of last byte read*/
                if ( enabled )
                        sample_buf = wave [ph >> 1];

                phase = ph ^ swap_banks; /* undo swapped banks*/
        }
        delay = time - end_time;
}

#include "Sound_Buffer.h"

#include "blargg_source.h"

#ifdef BLARGG_ENABLE_OPTIMIZER
        #include BLARGG_ENABLE_OPTIMIZER
#endif

/* BLIP BUFFER */

#define silent_buf_size 1

Blip_Buffer::Blip_Buffer()
{
        factor_       = INT_MAX;
        buffer_       = 0;
        buffer_size_  = 0;
        sample_rate_  = 0;
        bass_shift_   = 0;
        clock_rate_   = 0;
        bass_freq_    = 16;
        length_       = 0;
#ifndef FASTER_SOUND_HACK_NON_SILENCE
        last_non_silence = 0;
#endif

        clear();
}

#ifndef FASTER_SOUND_HACK_NON_SILENCE
#define blip_buffer_remove_silence(count) \
{ \
   if ( (b.last_non_silence -= count) < 0) \
      b.last_non_silence = 0; \
   \
   b.offset_ -= (uint32_t) count << BLIP_BUFFER_ACCURACY; \
}
#endif

Blip_Buffer::~Blip_Buffer()
{
        if ( buffer_size_ != silent_buf_size )
                free( buffer_ );
}

void Blip_Buffer::clear( int entire_buffer )
{
#ifndef FASTER_SOUND_HACK_NON_SILENCE
        last_non_silence = 0;
#endif
        offset_       = 0;
        reader_accum_ = 0;
        modified_     = 0;
        if ( buffer_ )
        {
                long count = (entire_buffer ? buffer_size_ : SAMPLES_AVAILABLE());
                memset( buffer_, 0, (count + blip_buffer_extra_) * sizeof (buf_t_) );
        }
}

const char * Blip_Buffer::set_sample_rate( long new_rate, int msec )
{
        if ( buffer_size_ == silent_buf_size )
                return "Internal (tried to resize Silent_Blip_Buffer)";

        /* start with maximum length that resampled time can represent*/
        long new_size = (ULONG_MAX >> BLIP_BUFFER_ACCURACY) - blip_buffer_extra_ - 64;
        if ( msec != 0)
        {
                long s = (new_rate * (msec + 1) + 999) / 1000;
                if ( s < new_size )
                        new_size = s;
        }

        if ( buffer_size_ != new_size )
        {
                void* p = realloc( buffer_, (new_size + blip_buffer_extra_) * sizeof *buffer_ );
                if ( !p )
                        return "Out of memory";
                buffer_ = (buf_t_*) p;
        }

        buffer_size_ = new_size;

        /* update things based on the sample rate*/
        sample_rate_ = new_rate;
        length_ = new_size * 1000 / new_rate - 1;

        /* update these since they depend on sample rate*/
        if ( clock_rate_ )
                clock_rate( clock_rate_ );
        bass_freq( bass_freq_ );

        clear();

        return 0;
}

uint32_t Blip_Buffer::clock_rate_factor( long rate ) const
{
        double ratio = (double) sample_rate_ / rate;
        int32_t factor = (int32_t) floor( ratio * (1L << BLIP_BUFFER_ACCURACY) + 0.5 );
        return (uint32_t) factor;
}

void Blip_Buffer::bass_freq( int freq )
{
        bass_freq_ = freq;
        int shift = 31;
        if ( freq > 0 )
        {
                shift = 13;
                long f = (freq << 16) / sample_rate_;
                while ( (f >>= 1) && --shift ) { }
        }
        bass_shift_ = shift;
}

void Blip_Buffer::end_frame( int32_t t )
{
        offset_ += t * factor_;
#ifndef FASTER_SOUND_HACK_NON_SILENCE
        if ( clear_modified() )
                last_non_silence = SAMPLES_AVAILABLE() + blip_buffer_extra_;
#endif
}

void Blip_Buffer::remove_samples( long count )
{
#ifndef FASTER_SOUND_HACK_NON_SILENCE
	if ( (last_non_silence -= count) < 0 )
		last_non_silence = 0;
#endif

	offset_ -= (uint32_t) count << BLIP_BUFFER_ACCURACY;

	/* copy remaining samples to beginning and clear old samples*/
	long remain = SAMPLES_AVAILABLE() + blip_buffer_extra_;
	memmove( buffer_, buffer_ + count, remain * sizeof *buffer_ );
	memset( buffer_ + remain, 0, count * sizeof *buffer_ );
}

/* Blip_Synth_*/

Blip_Synth_Fast_::Blip_Synth_Fast_()
{
        buf          = 0;
        last_amp     = 0;
        delta_factor = 0;
}

void Blip_Synth_Fast_::volume_unit( double new_unit )
{
        delta_factor = int (new_unit * (1L << BLIP_SAMPLE_BITS) + 0.5);
}

long Blip_Buffer::read_samples( int16_t * out, long count)
{
	BLIP_READER_BEGIN( reader, *this );
	BLIP_READER_ADJ_( reader, count );
	int16_t * BLIP_RESTRICT out_tmp = out + count;
	int32_t offset = (int32_t) -count;

	do
	{
		int32_t s = BLIP_READER_READ( reader );
		BLIP_READER_NEXT_IDX_( reader, offset );
		BLIP_CLAMP( s, s );
		out_tmp [offset] = (int16_t) s;
	}
	while ( ++offset );

	BLIP_READER_END( reader, *this );

	remove_samples( count );

#ifndef FASTER_SOUND_HACK_NON_SILENCE
		if ( (last_non_silence -= count) < 0 )
			last_non_silence = 0;
#endif

		return count;
}

uint32_t const subsample_mask = (1L << BLIP_BUFFER_ACCURACY) - 1;

void Blip_Buffer::save_state( blip_buffer_state_t* out )
{
        out->offset_       = offset_;
        out->reader_accum_ = reader_accum_;
        memcpy( out->buf, &buffer_ [offset_ >> BLIP_BUFFER_ACCURACY], sizeof out->buf );
}

void Blip_Buffer::load_state( blip_buffer_state_t const& in )
{
        clear( false );

        offset_       = in.offset_;
        reader_accum_ = in.reader_accum_;
        memcpy( buffer_, in.buf, sizeof in.buf );
}


#ifndef FASTER_SOUND_HACK_NON_SILENCE
uint32_t Blip_Buffer::non_silent() const
{
        return last_non_silence | (reader_accum_ >> 14);
}
#endif

/* Stereo_Buffer*/

int const stereo = 2;

Stereo_Buffer::Stereo_Buffer()
{
	samples_per_frame_      = 2;
	length_                 = 0;
	sample_rate_            = 0;
	channels_changed_count_ = 1;
	channel_types_          = 0;
	channel_count_          = 0;
	immediate_removal_      = true;

        chan.center = mixer_bufs [2] = &bufs_buffer [2];
        chan.left   = mixer_bufs [0] = &bufs_buffer [0];
        chan.right  = mixer_bufs [1] = &bufs_buffer [1];
        mixer_samples_read = 0;
}

Stereo_Buffer::~Stereo_Buffer() { }

const char * Stereo_Buffer::set_sample_rate( long rate, int msec )
{
        mixer_samples_read = 0;
        for ( int i = bufs_size; --i >= 0; )
                RETURN_ERR( bufs_buffer [i].set_sample_rate( rate, msec ) );
	sample_rate_ = bufs_buffer[0].sample_rate();
	length_ = bufs_buffer[0].length();
        return 0; 
}

void Stereo_Buffer::clock_rate( long rate )
{
        for ( int i = bufs_size; --i >= 0; )
                bufs_buffer [i].clock_rate( rate );
}

double Stereo_Buffer::real_ratio()
{
   return (double)bufs_buffer[0].clock_rate() * (double)bufs_buffer[0].clock_rate_factor(bufs_buffer[0].clock_rate()) / (1 << BLIP_BUFFER_ACCURACY);
}

void Stereo_Buffer::bass_freq( int bass )
{
        for ( int i = bufs_size; --i >= 0; )
                bufs_buffer [i].bass_freq( bass );
}


void Stereo_Buffer::clear()
{
        mixer_samples_read = 0;
        for ( int i = bufs_size; --i >= 0; )
                bufs_buffer [i].clear();
}

void Stereo_Buffer::end_frame( int32_t time )
{
        for ( int i = bufs_size; --i >= 0; )
                bufs_buffer [i].end_frame( time );
}

long Stereo_Buffer::read_samples( int16_t * out, long out_size )
{
        out_size = min( out_size, STEREO_BUFFER_SAMPLES_AVAILABLE());

        int pair_count = int (out_size >> 1);
        if ( pair_count )
        {
                mixer_read_pairs( out, pair_count );

                if (STEREO_BUFFER_SAMPLES_AVAILABLE() <= 0 || immediate_removal_ )
                {
                        for ( int i = bufs_size; --i >= 0; )
                        {
                                buf_t& b = bufs_buffer [i];
            #ifndef FASTER_SOUND_HACK_NON_SILENCE
                                /* TODO: might miss non-silence setting since it checks END of last read*/
                                if ( !b.non_silent() )
            {
                                        blip_buffer_remove_silence( mixer_samples_read );
            }
                                else
            #endif
                                        b.remove_samples( mixer_samples_read );
                        }
                        mixer_samples_read = 0;
                }
        }
        return out_size;
}


/* Stereo_Mixer*/

/* mixers use a single index value to improve performance on register-challenged processors*/
/* offset goes from negative to zero*/


void Stereo_Buffer::mixer_read_pairs( int16_t* out, int count )
{
	/* TODO: if caller never marks buffers as modified, uses mono*/
	/* except that buffer isn't cleared, so caller can encounter*/
	/* subtle problems and not realize the cause.*/
	mixer_samples_read += count;
#ifndef FASTER_SOUND_HACK_NON_SILENCE
	if ( mixer_bufs [0]->non_silent() | mixer_bufs [1]->non_silent() )
	{
#endif
		int16_t* BLIP_RESTRICT outtemp = out + count * stereo;

		/* do left + center and right + center separately to reduce register load*/
		Blip_Buffer* const* buf = &mixer_bufs [2];
		{
			--buf;
			--outtemp;

			BLIP_READER_BEGIN( side,   **buf );
			BLIP_READER_BEGIN( center, *mixer_bufs [2] );

			BLIP_READER_ADJ_( side,   mixer_samples_read );
			BLIP_READER_ADJ_( center, mixer_samples_read );

			int offset = -count;
			do
			{
				int s = center_reader_accum + side_reader_accum;
				s >>= 14;
				BLIP_READER_NEXT_IDX_( side,   offset );
				BLIP_READER_NEXT_IDX_( center, offset );
				BLIP_CLAMP( s, s );

				++offset; /* before write since out is decremented to slightly before end*/
				outtemp [offset * stereo] = (int16_t) s;
			}while ( offset );

			BLIP_READER_END( side,   **buf );
		}
		{
			--buf;
			--outtemp;

			BLIP_READER_BEGIN( side,   **buf );
			BLIP_READER_BEGIN( center, *mixer_bufs [2] );

			BLIP_READER_ADJ_( side,   mixer_samples_read );
			BLIP_READER_ADJ_( center, mixer_samples_read );

			int offset = -count;
			do
			{
				int s = center_reader_accum + side_reader_accum;
				s >>= 14;
				BLIP_READER_NEXT_IDX_( side,   offset );
				BLIP_READER_NEXT_IDX_( center, offset );
				BLIP_CLAMP( s, s );

				++offset; /* before write since out is decremented to slightly before end*/
				outtemp [offset * stereo] = (int16_t) s;
			}while ( offset );

			BLIP_READER_END( side,   **buf );

			/* only end center once*/
			BLIP_READER_END( center, *mixer_bufs [2] );
		}
#ifndef FASTER_SOUND_HACK_NON_SILENCE
	}
	else
	{
		BLIP_READER_BEGIN( center, *mixer_bufs [2] );
		BLIP_READER_ADJ_( center, mixer_samples_read );

		typedef int16_t stereo_blip_sample_t [stereo];
		stereo_blip_sample_t* BLIP_RESTRICT outtemp = (stereo_blip_sample_t*) out + count;
		int offset = -count;
		do
		{
			int s = BLIP_READER_READ( center );
			BLIP_READER_NEXT_IDX_( center, offset );
			BLIP_CLAMP( s, s );

			outtemp [offset] [0] = (int16_t) s;
			outtemp [offset] [1] = (int16_t) s;
		}
		while ( ++offset );
		BLIP_READER_END( center, *mixer_bufs [2] );
	}
#endif
}

int const fixed_shift = 12;
#define TO_FIXED( f )   int ((f) * ((int) 1 << fixed_shift))
#define FROM_FIXED( f ) ((f) >> fixed_shift)

int const max_read = 2560; /* determines minimum delay*/

void Effects_Buffer::clear()
{
}

void Effects_Buffer::mixer_read_pairs( int16_t * out, int count )
{
	/* TODO: if caller never marks buffers as modified, uses mono
	   except that buffer isn't cleared, so caller can encounter
	   subtle problems and not realize the cause. */

	mixer_samples_read += count;
#ifndef FASTER_SOUND_HACK_NON_SILENCE
	if ( mixer_bufs [0]->non_silent() | mixer_bufs [1]->non_silent() )
	{
#endif
		int16_t * BLIP_RESTRICT outtemp = out + count * stereo;

		/* do left + center and right + center separately to reduce register load*/
		Blip_Buffer* const* buf = &mixer_bufs [2];
		{
			--buf;
			--outtemp;

			BLIP_READER_BEGIN( side,   **buf );
			BLIP_READER_BEGIN( center, *mixer_bufs [2] );

			BLIP_READER_ADJ_( side,   mixer_samples_read );
			BLIP_READER_ADJ_( center, mixer_samples_read );

			int offset = -count;
			do
			{
				int s = center_reader_accum + side_reader_accum;
				s >>= 14;
				BLIP_READER_NEXT_IDX_( side,   offset );
				BLIP_READER_NEXT_IDX_( center, offset );
				BLIP_CLAMP( s, s );

				++offset; /* before write since out is decremented to slightly before end */
				outtemp [offset * stereo] = (int16_t) s;
			}while ( offset );

			BLIP_READER_END( side,   **buf );
		}
		{
			--buf;
			--outtemp;

			BLIP_READER_BEGIN( side,   **buf );
			BLIP_READER_BEGIN( center, *mixer_bufs [2] );

			BLIP_READER_ADJ_( side,   mixer_samples_read );
			BLIP_READER_ADJ_( center, mixer_samples_read );

			int offset = -count;
			do
			{
				int s = center_reader_accum + side_reader_accum;
				s >>= 14;
				BLIP_READER_NEXT_IDX_( side,   offset );
				BLIP_READER_NEXT_IDX_( center, offset );
				BLIP_CLAMP( s, s );

				++offset; /* before write since out is decremented to slightly before end*/
				outtemp [offset * stereo] = (int16_t) s;
			}while ( offset );

			BLIP_READER_END( side,   **buf );

			/* only end center once*/
			BLIP_READER_END( center, *mixer_bufs [2] );
		}
#ifndef FASTER_SOUND_HACK_NON_SILENCE
	}
	else
	{
		BLIP_READER_BEGIN( center, *mixer_bufs [2] );
		BLIP_READER_ADJ_( center, mixer_samples_read );

		typedef int16_t stereo_blip_sample_t [stereo];
		stereo_blip_sample_t* BLIP_RESTRICT outtemp = (stereo_blip_sample_t*) out + count;
		int offset = -count;
		do
		{
			int s = BLIP_READER_READ( center );
			BLIP_READER_NEXT_IDX_( center, offset );
			BLIP_CLAMP( s, s );

			outtemp [offset] [0] = (int16_t) s;
			outtemp [offset] [1] = (int16_t) s;
		}
		while ( ++offset );
		BLIP_READER_END( center, *mixer_bufs [2] );
	}
#endif
}

Effects_Buffer::Effects_Buffer( int max_bufs, long echo_size_ )
{
	/* from Multi_Buffer */
	samples_per_frame_      = stereo;
	length_                 = 0;
	sample_rate_            = 0;
	channels_changed_count_ = 1;
	channel_types_          = 0;
	channel_count_          = 0;
	immediate_removal_      = true;

        echo_size   = max( max_read * (long) stereo, echo_size_ & ~1 );
        clock_rate_ = 0;
        bass_freq_  = 90;
        bufs_buffer       = 0;
        bufs_size   = 0;
        bufs_max    = max( max_bufs, (int) extra_chans );
        no_echo     = true;
        no_effects  = true;

        /* defaults */
        config_.enabled   = false;
        config_.delay [0] = 120;
        config_.delay [1] = 122;
        config_.feedback  = 0.2f;
        config_.treble    = 0.4f;

        static float const sep = 0.8f;
        config_.side_chans [0].pan = -sep;
        config_.side_chans [1].pan = +sep;
        config_.side_chans [0].vol = 1.0f;
        config_.side_chans [1].vol = 1.0f;

        memset( &s, 0, sizeof s );
        echo_pos       = 0;
        s.low_pass [0] = 0;
        s.low_pass [1] = 0;
        mixer_samples_read = 0;

        for ( int i = bufs_size; --i >= 0; )
                bufs_buffer [i].clear();
        if ( echo.size() )
                memset( echo.begin(), 0, echo.size() * sizeof echo [0] );
}

Effects_Buffer::~Effects_Buffer()
{
        if ( bufs_buffer )
        {
                for ( int i = bufs_size; --i >= 0; )
                        bufs_buffer [i].~buf_t();
                free( bufs_buffer );
                bufs_buffer = 0;
        }
        bufs_size = 0;
}

/* avoid using new [] */
const char * Effects_Buffer::new_bufs( int size )
{
        bufs_buffer = (buf_t*) malloc( size * sizeof *bufs_buffer );
        CHECK_ALLOC( bufs_buffer );
        for ( int i = 0; i < size; i++ )
                new (bufs_buffer + i) buf_t;
        bufs_size = size;
        return 0;
}

const char * Effects_Buffer::set_sample_rate( long rate, int msec )
{
        /* extra to allow farther past-the-end pointers */
        mixer_samples_read = 0;
        RETURN_ERR( echo.resize( echo_size + stereo ) );
	sample_rate_ = rate;
	length_ = msec;
	return 0;
}

void Effects_Buffer::clock_rate( long rate )
{
        clock_rate_ = rate;
        for ( int i = bufs_size; --i >= 0; )
                bufs_buffer [i].clock_rate( clock_rate_ );
}

void Effects_Buffer::bass_freq( int freq )
{
        bass_freq_ = freq;
        for ( int i = bufs_size; --i >= 0; )
                bufs_buffer [i].bass_freq( bass_freq_ );
}

const char * Effects_Buffer::set_channel_count( int count, int const* types )
{
	channel_count_ = count;
	channel_types_ = types;

        if ( bufs_buffer )
        {
                for ( int i = bufs_size; --i >= 0; )
                        bufs_buffer [i].~buf_t();
                free( bufs_buffer );
                bufs_buffer = 0;
        }
        bufs_size = 0;

        mixer_samples_read = 0;

        RETURN_ERR( chans.resize( count + extra_chans ) );

        RETURN_ERR( new_bufs( min( bufs_max, count + extra_chans ) ) );

        for ( int i = bufs_size; --i >= 0; )
                RETURN_ERR( bufs_buffer [i].set_sample_rate( sample_rate_, length_ ));

        for ( int i = chans.size(); --i >= 0; )
        {
                chan_t& ch = chans [i];
                ch.cfg.vol      = 1.0f;
                ch.cfg.pan      = 0.0f;
                ch.cfg.surround = false;
                ch.cfg.echo     = false;
        }

        /* side channels with echo */
        chans [2].cfg.echo = true;
        chans [3].cfg.echo = true;

        clock_rate( clock_rate_ );
        bass_freq( bass_freq_ );
        apply_config();
        echo_pos       = 0;
        s.low_pass [0] = 0;
        s.low_pass [1] = 0;
        mixer_samples_read = 0;

        for ( int i = bufs_size; --i >= 0; )
                bufs_buffer [i].clear();
        if ( echo.size() )
                memset( echo.begin(), 0, echo.size() * sizeof echo [0] );

        return 0;
}

channel_t Effects_Buffer::channel( int i )
{
        i += extra_chans;
        return chans [i].channel;
}


/* Configuration*/

/* 3 wave positions with/without surround, 2 multi (one with same config as wave)*/
int const simple_bufs = 3 * 2 + 2 - 1;

Simple_Effects_Buffer::Simple_Effects_Buffer() :
        Effects_Buffer( extra_chans + simple_bufs, 18 * 1024L )
{
        config_.echo     = 0.20f;
        config_.stereo   = 0.20f;
        config_.surround = true;
        config_.enabled  = false;
}

void Simple_Effects_Buffer::apply_config()
{
        Effects_Buffer::config_t& c = Effects_Buffer::config();

        c.enabled = config_.enabled;
        if ( c.enabled )
        {
                c.delay [0] = 120;
                c.delay [1] = 122;
                c.feedback  = config_.echo * 0.7f;
                c.treble    = 0.6f - 0.3f * config_.echo;

                float sep = config_.stereo + 0.80f;
                if ( sep > 1.0f )
                        sep = 1.0f;

                c.side_chans [0].pan = -sep;
                c.side_chans [1].pan = +sep;

                for ( int i = channel_count_; --i >= 0; )
                {
                        chan_config_t& ch = Effects_Buffer::chan_config( i );

                        ch.pan      = 0.0f;
                        ch.surround = config_.surround;
                        ch.echo     = false;

                        int const type = (channel_types_ ? channel_types_ [i] : 0);
                        if ( !(type & noise_type) )
                        {
                                int index = (type & type_index_mask) % 6 - 3;
                                if ( index < 0 )
                                {
                                        index += 3;
                                        ch.surround = false;
                                        ch.echo     = true;
                                }
                                if ( index >= 1 )
                                {
                                        ch.pan = config_.stereo;
                                        if ( index == 1 )
                                                ch.pan = -ch.pan;
                                }
                        }
                        else if ( type & 1 )
                        {
                                ch.surround = false;
                        }
                }
        }

        Effects_Buffer::apply_config();
}

void Effects_Buffer::apply_config()
{
	int i;

	if ( !bufs_size )
		return;

	s.treble = TO_FIXED( config_.treble );

	bool echo_dirty = false;

	int old_feedback = s.feedback;
	s.feedback = TO_FIXED( config_.feedback );
	if ( !old_feedback && s.feedback )
		echo_dirty = true;

	/* delays*/
	for ( i = stereo; --i >= 0; )
	{
		long delay = config_.delay [i] * sample_rate_ / 1000 * stereo;
		delay = max( delay, long (max_read * stereo) );
		delay = min( delay, long (echo_size - max_read * stereo) );
		if ( s.delay [i] != delay )
		{
			s.delay [i] = delay;
			echo_dirty = true;
		}
	}

	/* side channels*/
	for ( i = 2; --i >= 0; )
	{
		chans [i+2].cfg.vol = chans [i].cfg.vol = config_.side_chans [i].vol * 0.5f;
		chans [i+2].cfg.pan = chans [i].cfg.pan = config_.side_chans [i].pan;
	}

	/* convert volumes*/
	for ( i = chans.size(); --i >= 0; )
	{
		chan_t& ch = chans [i];
		ch.vol [0] = TO_FIXED( ch.cfg.vol - ch.cfg.vol * ch.cfg.pan );
		ch.vol [1] = TO_FIXED( ch.cfg.vol + ch.cfg.vol * ch.cfg.pan );
		if ( ch.cfg.surround )
			ch.vol [0] = -ch.vol [0];
	}

	/*Begin of assign buffers*/
	/* assign channels to buffers*/
	int buf_count = 0;
	for ( int i = 0; i < (int) chans.size(); i++ )
	{
		/* put second two side channels at end to give priority to main channels*/
		/* in case closest matching is necessary*/
		int x = i;
		if ( i > 1 )
			x += 2;
		if ( x >= (int) chans.size() )
			x -= (chans.size() - 2);
		chan_t& ch = chans [x];

		int b = 0;
		for ( ; b < buf_count; b++ )
		{
			if (    ch.vol [0] == bufs_buffer [b].vol [0] &&
					ch.vol [1] == bufs_buffer [b].vol [1] &&
					(ch.cfg.echo == bufs_buffer [b].echo || !s.feedback) )
				break;
		}

		if ( b >= buf_count )
		{
			if ( buf_count < bufs_max )
			{
				bufs_buffer [b].vol [0] = ch.vol [0];
				bufs_buffer [b].vol [1] = ch.vol [1];
				bufs_buffer [b].echo    = ch.cfg.echo;
				buf_count++;
			}
			else
			{
				/* TODO: this is a mess, needs refinement*/
				b = 0;
				int best_dist = TO_FIXED( 8 );
				for ( int h = buf_count; --h >= 0; )
				{
#define CALC_LEVELS( vols, sum, diff, surround ) \
					int sum, diff;\
					bool surround = false;\
					{\
						int vol_0 = vols [0];\
						if ( vol_0 < 0 ) vol_0 = -vol_0, surround = true;\
						int vol_1 = vols [1];\
						if ( vol_1 < 0 ) vol_1 = -vol_1, surround = true;\
						sum  = vol_0 + vol_1;\
						diff = vol_0 - vol_1;\
					}
					CALC_LEVELS( ch.vol,       ch_sum,  ch_diff,  ch_surround );
					CALC_LEVELS( bufs_buffer [h].vol, buf_sum, buf_diff, buf_surround );

					int dist = abs( ch_sum - buf_sum ) + abs( ch_diff - buf_diff );

					if ( ch_surround != buf_surround )
						dist += TO_FIXED( 1 ) / 2;

					if ( s.feedback && ch.cfg.echo != bufs_buffer [h].echo )
						dist += TO_FIXED( 1 ) / 2;

					if ( best_dist > dist )
					{
						best_dist = dist;
						b = h;
					}
				}
			}
		}

		ch.channel.center = &bufs_buffer [b];
	}
	/*End of assign buffers*/

	/* set side channels*/
	for ( i = chans.size(); --i >= 0; )
	{
		chan_t& ch = chans [i];
		ch.channel.left  = chans [ch.cfg.echo*2  ].channel.center;
		ch.channel.right = chans [ch.cfg.echo*2+1].channel.center;
	}

	bool old_echo = !no_echo && !no_effects;

	/* determine whether effects and echo are needed at all*/
	no_effects = true;
	no_echo    = true;
	for ( i = chans.size(); --i >= extra_chans; )
	{
		chan_t& ch = chans [i];
		if ( ch.cfg.echo && s.feedback )
			no_echo = false;

		if ( ch.vol [0] != TO_FIXED( 1 ) || ch.vol [1] != TO_FIXED( 1 ) )
			no_effects = false;
	}
	if ( !no_echo )
		no_effects = false;

	if (    chans [0].vol [0] != TO_FIXED( 1 ) ||
			chans [0].vol [1] != TO_FIXED( 0 ) ||
			chans [1].vol [0] != TO_FIXED( 0 ) ||
			chans [1].vol [1] != TO_FIXED( 1 ) )
		no_effects = false;

	if ( !config_.enabled )
		no_effects = true;

	if ( no_effects )
	{
		for ( i = chans.size(); --i >= 0; )
		{
			chan_t& ch = chans [i];
			ch.channel.center = &bufs_buffer [2];
			ch.channel.left   = &bufs_buffer [0];
			ch.channel.right  = &bufs_buffer [1];
		}
	}

	mixer_bufs [0] = &bufs_buffer [0];
	mixer_bufs [1] = &bufs_buffer [1];
	mixer_bufs [2] = &bufs_buffer [2];

	if ( echo_dirty || (!old_echo && (!no_echo && !no_effects)) )
	{
		if ( echo.size() )
			memset( echo.begin(), 0, echo.size() * sizeof echo [0] );
	}

	channels_changed();
}

/* Mixing */

void Effects_Buffer::end_frame( int32_t time )
{
        for ( int i = bufs_size; --i >= 0; )
                bufs_buffer [i].end_frame( time );
}

long Effects_Buffer::read_samples( int16_t * out, long out_size )
{
        out_size = min( out_size, STEREO_BUFFER_SAMPLES_AVAILABLE());

        int pair_count = int (out_size >> 1);
        if ( pair_count )
        {
                if ( no_effects )
                {
                        mixer_read_pairs( out, pair_count );
                }
                else
                {
                        int pairs_remain = pair_count;
                        do
                        {
                                /* mix at most max_read pairs at a time*/
                                int count = max_read;
                                if ( count > pairs_remain )
                                        count = pairs_remain;

                                if ( no_echo )
                                {
                                        /* optimization: clear echo here to keep mix_effects() a leaf function*/
                                        echo_pos = 0;
                                        memset( echo.begin(), 0, count * stereo * sizeof echo [0] );
                                }

                                mix_effects( out, count );

                                int new_echo_pos = echo_pos + count * stereo;
                                if ( new_echo_pos >= echo_size )
                                        new_echo_pos -= echo_size;
                                echo_pos = new_echo_pos;

                                out += count * stereo;
                                mixer_samples_read += count;
                                pairs_remain -= count;
                        }
                        while ( pairs_remain );
                }

                if ( STEREO_BUFFER_SAMPLES_AVAILABLE() <= 0 || immediate_removal_ )
                {
                        for ( int i = bufs_size; --i >= 0; )
                        {
                                buf_t& b = bufs_buffer [i];
                                /* TODO: might miss non-silence settling since it checks END of last read*/
            #ifdef FASTER_SOUND_HACK_NON_SILENCE
                                b.remove_samples( mixer_samples_read );
            #else
                                if ( b.non_silent() )
                                        b.remove_samples( mixer_samples_read );
                                else
            {
                                        blip_buffer_remove_silence( mixer_samples_read );
            }
            #endif
                        }
                        mixer_samples_read = 0;
                }
        }
        return out_size;
}

void Effects_Buffer::mix_effects( int16_t * out_, int pair_count )
{
	typedef int stereo_fixed_t [stereo];

	/* add channels with echo, do echo, add channels without echo, then convert to 16-bit and output*/
	int echo_phase = 1;
	do
	{
		/* mix any modified buffers*/
		int bufs_remain;

		buf_t* buf = bufs_buffer;
		bufs_remain = bufs_size;
		do
		{
#ifdef FASTER_SOUND_HACK_NON_SILENCE
			if ( ( buf->echo == !!echo_phase ) )
#else
				if ( buf->non_silent() && ( buf->echo == !!echo_phase ) )
#endif
				{
					int vol_0, vol_1, count, remain;

					stereo_fixed_t* BLIP_RESTRICT out = (stereo_fixed_t*) &echo [echo_pos];
					BLIP_READER_BEGIN( in, *buf );
					BLIP_READER_ADJ_( in, mixer_samples_read );
					vol_0 = buf->vol [0];
					vol_1 = buf->vol [1];

					count = unsigned (echo_size - echo_pos) / stereo;
					remain = pair_count;

					if ( count > remain )
						count = remain;
					do
					{
						remain -= count;
						BLIP_READER_ADJ_( in, count );

						out += count;
						int offset = -count;
						do
						{
							int s = BLIP_READER_READ( in );
							BLIP_READER_NEXT_IDX_( in, offset );

							out [offset] [0] += s * vol_0;
							out [offset] [1] += s * vol_1;
							offset++;
						}
						while ( offset );

						out = (stereo_fixed_t*) echo.begin();
						count = remain;
					}
					while ( remain );

					BLIP_READER_END( in, *buf );
				}
			buf++;
			bufs_remain--;
		}
		while (bufs_remain);

		/* add echo*/
		if ( echo_phase && !no_echo )
		{
			int feedback, treble, i;

			feedback = s.feedback;
			treble   = s.treble;

			i = 1;

			do
			{
				int low_pass, *echo_end, out_offset, remain;

				low_pass = s.low_pass [i];

				echo_end = &echo [echo_size + i];

				int const* BLIP_RESTRICT in_pos = &echo [echo_pos + i];

				out_offset = echo_pos + i + s.delay [i];

				if ( out_offset >= echo_size )
					out_offset -= echo_size;

				int * BLIP_RESTRICT out_pos = &echo [out_offset];

				/* break into up to three chunks to avoid having to handle wrap-around*/
				/* in middle of core loop*/
				remain = pair_count;

				do
				{
					int count, offset;

					int const * pos = in_pos;

					if ( pos < out_pos )
						pos = out_pos;

					count = unsigned ((char*) echo_end - (char const*) pos) /
						unsigned (stereo * sizeof(int));
					if ( count > remain )
						count = remain;
					remain -= count;

					in_pos  += count * stereo;
					out_pos += count * stereo;
					offset = -count;

					do
					{
						low_pass += FROM_FIXED( in_pos [offset * stereo] - low_pass ) * treble;
						out_pos [offset * stereo] = FROM_FIXED( low_pass ) * feedback;
						offset++;
					}
					while(offset);

					if(in_pos >= echo_end)
						in_pos -= echo_size;
					if(out_pos >= echo_end)
						out_pos -= echo_size;
				}
				while ( remain );

				s.low_pass [i] = low_pass;
				i--;
			}
			while ( i >= 0 );
		}
		echo_phase--;
	}
	while ( echo_phase >= 0 );

	/* clamp to 16 bits*/
	stereo_fixed_t const* BLIP_RESTRICT in = (stereo_fixed_t*) &echo [echo_pos];
	typedef int16_t stereo_blip_sample_t [stereo];
	stereo_blip_sample_t* BLIP_RESTRICT out = (stereo_blip_sample_t*) out_;
	int count = unsigned (echo_size - echo_pos) / (unsigned) stereo;
	int remain = pair_count;
	if ( count > remain )
		count = remain;
	do
	{
		int offset;
		remain -= count;
		in  += count;
		out += count;
		offset = -count;
		do
		{
			int in_0, in_1;

			in_0 = FROM_FIXED( in [offset] [0] );
			in_1 = FROM_FIXED( in [offset] [1] );

			BLIP_CLAMP( in_0, in_0 );
			out [offset] [0] = (int16_t) in_0;

			BLIP_CLAMP( in_1, in_1 );
			out [offset] [1] = (int16_t) in_1;
		}
		while ( ++offset );

		in = (stereo_fixed_t*) echo.begin();
		count = remain;
	}
	while ( remain );
}
