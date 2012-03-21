/* Multi-channel sound buffer interface, and basic mono and stereo buffers */

#ifndef MULTI_BUFFER_H
#define MULTI_BUFFER_H

#define STEREO 2
#define EXTRA_CHANS 4
#define BUFS_SIZE 3

#include "../System.h"

#include "blargg_common.h"

/* BLIP BUFFER */

struct blip_buffer_state_t;

class Blip_Buffer
{
	public:
	// Sets output sample rate and buffer length in milliseconds (1/1000 sec, defaults
	// to 1/4 second) and clears buffer. If there isn't enough memory, leaves buffer
	// untouched and returns "Out of memory", otherwise returns NULL.
	const char * set_sample_rate( long samples_per_sec, int msec_length = 1000 / 4 );

	/* Removes all available samples and clear buffer to silence.*/
	void clear( void);

	/* Experimental features*/

	/* Saves state, including high-pass filter and tails of last deltas.*/
	/* All samples must have been read from buffer before calling this.*/
	void save_state( blip_buffer_state_t* out );

	// Loads state. State must have been saved from Blip_Buffer with same
	// settings during same run of program. States can NOT be stored on disk.
	// Clears buffer before loading state.
	void load_state( blip_buffer_state_t const& in );

	/* not documented yet*/
	uint32_t clock_rate_factor( long clock_rate ) const;
	long clock_rate_;

	int length_;		/* Length of buffer in milliseconds*/
	long sample_rate_;	/* Current output sample rate*/
	uint32_t factor_;
	uint32_t offset_;
	int32_t * buffer_;
	int32_t buffer_size_;
	int32_t reader_accum_;
	Blip_Buffer();
	~Blip_Buffer();
	private:
	Blip_Buffer( const Blip_Buffer& );
	Blip_Buffer& operator = ( const Blip_Buffer& );
};

/* Number of bits in resample ratio fraction. Higher values give a more accurate 
   ratio but reduce maximum buffer size.*/
#define BLIP_BUFFER_ACCURACY 16

/* Number bits in phase offset. Fewer than 6 bits (64 phase offsets) results in
   noticeable broadband noise when synthesizing high frequency square waves.
   Affects size of Blip_Synth objects since they store the waveform directly. */
#define BLIP_PHASE_BITS 8

/* Internal*/
#define BLIP_WIDEST_IMPULSE_ 16
#define BLIP_BUFFER_EXTRA_ 18
#define BLIP_RES 256
#define BLIP_RES_MIN_ONE 255

#define BLIP_SAMPLE_BITS 30

/* Constant value to use instead of BLIP_READER_BASS(), for slightly more optimal
   code at the cost of having no bass control */
#define BLIP_READER_DEFAULT_BASS 9

class Blip_Synth
{
	public:
	Blip_Buffer* buf;
	int delta_factor;

	Blip_Synth();

	/* Sets overall volume of waveform */
	void volume( double v ) { delta_factor = int ((v * 1.0) * (1L << BLIP_SAMPLE_BITS) + 0.5); }

	// Low-level interface
	// Adds an amplitude transition of specified delta, optionally into specified buffer
	// rather than the one set with output(). Delta can be positive or negative.
	// The actual change in amplitude is delta * (volume / range)
	void offset( int32_t, int delta, Blip_Buffer* ) const;

	/* Works directly in terms of fractional output samples. Contact author for more info.*/
	void offset_resampled( uint32_t, int delta, Blip_Buffer* ) const;

	/* Same as offset(), except code is inlined for higher performance*/
	void offset_inline( int32_t t, int delta, Blip_Buffer* buf ) const {
		offset_resampled( t * buf->factor_ + buf->offset_, delta, buf );
	}
	void offset_inline( int32_t t, int delta ) const {
		offset_resampled( t * buf->factor_ + buf->offset_, delta, buf );
	}
};

/* Optimized reading from Blip_Buffer, for use in custom sample output*/

/* Begins reading from buffer. Name should be unique to the current block.*/
#define BLIP_READER_BEGIN( name, blip_buffer ) \
        const int32_t * name##_reader_buf = (blip_buffer).buffer_;\
        int32_t name##_reader_accum = (blip_buffer).reader_accum_


/* Advances to next sample*/
#define BLIP_READER_NEXT( name, bass ) \
        (void) (name##_reader_accum += *name##_reader_buf++ - (name##_reader_accum >> (bass)))

/* Ends reading samples from buffer. The number of samples read must now be removed
   using Blip_Buffer::remove_samples(). */
#define BLIP_READER_END( name, blip_buffer ) \
        (void) ((blip_buffer).reader_accum_ = name##_reader_accum)

#define BLIP_READER_ADJ_( name, offset ) (name##_reader_buf += offset)

#define BLIP_READER_NEXT_IDX_( name, idx ) {\
        name##_reader_accum -= name##_reader_accum >> BLIP_READER_DEFAULT_BASS;\
        name##_reader_accum += name##_reader_buf [(idx)];\
}

#define BLIP_READER_NEXT_RAW_IDX_( name, idx ) {\
        name##_reader_accum -= name##_reader_accum >> BLIP_READER_DEFAULT_BASS; \
        name##_reader_accum += *(int32_t const*) ((char const*) name##_reader_buf + (idx)); \
}

#if defined (_M_IX86) || defined (_M_IA64) || defined (__i486__) || \
                defined (__x86_64__) || defined (__ia64__) || defined (__i386__)
        #define BLIP_CLAMP_( in ) in < -0x8000 || 0x7FFF < in
#else
        #define BLIP_CLAMP_( in ) (int16_t) in != in
#endif

/* Clamp sample to int16_t range */
#define BLIP_CLAMP( sample, out ) { if ( BLIP_CLAMP_( (sample) ) ) (out) = ((sample) >> 24) ^ 0x7FFF; }

struct blip_buffer_state_t
{
        uint32_t offset_;
        int32_t reader_accum_;
        int32_t buf [BLIP_BUFFER_EXTRA_];
};

INLINE void Blip_Synth::offset_resampled( uint32_t time, int delta, Blip_Buffer* blip_buf ) const
{
	int32_t left, right, phase;
	int32_t *buf;

	delta *= delta_factor;
	buf = blip_buf->buffer_ + (time >> BLIP_BUFFER_ACCURACY);
	phase = (int) (time >> (BLIP_BUFFER_ACCURACY - BLIP_PHASE_BITS) & BLIP_RES_MIN_ONE);

	left = buf [0] + delta;

	/* Kind of crappy, but doing shift after multiply results in overflow.
	Alternate way of delaying multiply by delta_factor results in worse
	sub-sample resolution.*/
	right = (delta >> BLIP_PHASE_BITS) * phase;

	left  -= right;
	right += buf [1];

	buf [0] = left;
	buf [1] = right;
}

INLINE void Blip_Synth::offset( int32_t t, int delta, Blip_Buffer* buf ) const
{
        offset_resampled( t * buf->factor_ + buf->offset_, delta, buf );
}

/* 1/4th of a second */
#define BLIP_DEFAULT_LENGTH 250

#define	WAVE_TYPE	0x100
#define NOISE_TYPE	0x200
#define MIXED_TYPE	WAVE_TYPE | NOISE_TYPE
#define TYPE_INDEX_MASK	0xFF

struct channel_t {
	Blip_Buffer* center;
	Blip_Buffer* left;
	Blip_Buffer* right;
};

#endif
