// Multi-channel sound buffer interface, and basic mono and stereo buffers

// Blip_Buffer 0.4.1
#ifndef MULTI_BUFFER_H
#define MULTI_BUFFER_H

#include "../System.h"

#include "blargg_common.h"

/* BLIP BUFFER */

// Output samples are 16-bit signed, with a range of -32768 to 32767
enum { blip_sample_max = 32767 };

struct blip_buffer_state_t;

class Blip_Buffer
{
	public:
	uint32_t non_silent() const;

	// Sets output sample rate and buffer length in milliseconds (1/1000 sec, defaults
	// to 1/4 second) and clears buffer. If there isn't enough memory, leaves buffer
	// untouched and returns "Out of memory", otherwise returns NULL.
	const char * set_sample_rate( long samples_per_sec, int msec_length = 1000 / 4 );

	// Sets number of source time units per second
	void clock_rate( long clocks_per_sec );

	// Ends current time frame of specified duration and makes its samples available
	// (along with any still-unread samples) for reading with read_samples(). Begins
	// a new time frame at the end of the current frame.
	void end_frame( int32_t time );

	// Reads at most 'max_samples' out of buffer into 'dest', removing them from
	// the buffer. Returns number of samples actually read and removed. If stereo is
	// true, increments 'dest' one extra time after writing each sample, to allow
	// easy interleving of two channels into a stereo output buffer.
	long read_samples( int16_t* dest, long count);

	// Additional features

	// Removes all available samples and clear buffer to silence. If 'entire_buffer' is
	// false, just clears out any samples waiting rather than the entire buffer.
	void clear( int entire_buffer = 1 );

	// Number of samples available for reading with read_samples()
	long samples_avail() const;

	// Removes 'count' samples from those waiting to be read
	void remove_samples( long count );

	// Sets frequency high-pass filter frequency, where higher values reduce bass more
	void bass_freq( int frequency );

	// Current output sample rate
	long sample_rate() const;

	// Length of buffer in milliseconds
	int length() const;

	// Number of source time units per second
	long clock_rate() const;

	// Experimental features

	// Saves state, including high-pass filter and tails of last deltas.
	// All samples must have been read from buffer before calling this.
	void save_state( blip_buffer_state_t* out );

	// Loads state. State must have been saved from Blip_Buffer with same
	// settings during same run of program. States can NOT be stored on disk.
	// Clears buffer before loading state.
	void load_state( blip_buffer_state_t const& in );

	// Number of samples delay from synthesis to samples read out
	//int output_latency() const;

	// Number of raw samples that can be mixed within frame of specified duration.
	//long count_samples( int32_t duration ) const;

	// Mixes in 'count' samples from 'buf_in'
	//void mix_samples( int16_t const* buf_in, long count );


	// Signals that sound has been added to buffer. Could be done automatically in
	// Blip_Synth, but that would affect performance more, as you can arrange that
	// this is called only once per time frame rather than for every delta.
	void set_modified() { modified_ = this; }

	// not documented yet
	Blip_Buffer* clear_modified() { Blip_Buffer* b = modified_; modified_ = 0; return b; }
	void remove_silence( long count );
	uint32_t resampled_time( int32_t t ) const { return t * factor_ + offset_; }
	uint32_t clock_rate_factor( long clock_rate ) const;
	public:
	Blip_Buffer();
	~Blip_Buffer();

	// Deprecated
	const char * sample_rate( long r ) { return set_sample_rate( r ); }
	const char * sample_rate( long r, int msec ) { return set_sample_rate( r, msec ); }
	private:
	// noncopyable
	Blip_Buffer( const Blip_Buffer& );
	Blip_Buffer& operator = ( const Blip_Buffer& );
	public:
	typedef int32_t buf_t_;
	uint32_t factor_;
	uint32_t offset_;
	buf_t_* buffer_;
	int32_t buffer_size_;
	int32_t reader_accum_;
	int bass_shift_;
	Blip_Buffer* modified_; // non-zero = true (more optimal than using bool, heh)
	int32_t last_non_silence;
	private:
	long sample_rate_;
	long clock_rate_;
	int bass_freq_;
	int length_;
};

#ifdef HAVE_CONFIG_H
        #include "config.h"
#endif

// Number of bits in resample ratio fraction. Higher values give a more accurate ratio
// but reduce maximum buffer size.
#define BLIP_BUFFER_ACCURACY 16

// Number bits in phase offset. Fewer than 6 bits (64 phase offsets) results in
// noticeable broadband noise when synthesizing high frequency square waves.
// Affects size of Blip_Synth objects since they store the waveform directly.
#ifndef BLIP_PHASE_BITS
                #define BLIP_PHASE_BITS 8
#endif

// Internal
#define BLIP_WIDEST_IMPULSE_ 16
#define BLIP_BUFFER_EXTRA_ 18
int const blip_res = 1 << BLIP_PHASE_BITS;

class Blip_Synth_Fast_
{
	public:
	Blip_Buffer* buf;
	int last_amp;
	int delta_factor;

	void volume_unit( double );
	Blip_Synth_Fast_();
};

// Quality level, better = slower. In general, use blip_good_quality.
const int blip_med_quality  = 8;
const int blip_good_quality = 12;
const int blip_high_quality = 16;

// Range specifies the greatest expected change in amplitude. Calculate it
// by finding the difference between the maximum and minimum expected
// amplitudes (max - min).
template<int quality,int range>
class Blip_Synth {
	public:
		// Sets overall volume of waveform
		void volume( double v ) { impl.volume_unit( v * (1.0 / (range < 0 ? -range : range)) ); }

		void output( Blip_Buffer* b )               { impl.buf = b; impl.last_amp = 0; }

		// Updates amplitude of waveform at given time. Using this requires a separate
		// Blip_Synth for each waveform.
		void update( int32_t time, int amplitude );

		// Low-level interface

		// Adds an amplitude transition of specified delta, optionally into specified buffer
		// rather than the one set with output(). Delta can be positive or negative.
		// The actual change in amplitude is delta * (volume / range)
		void offset( int32_t, int delta, Blip_Buffer* ) const;

		// Works directly in terms of fractional output samples. Contact author for more info.
		void offset_resampled( uint32_t, int delta, Blip_Buffer* ) const;

		// Same as offset(), except code is inlined for higher performance
		void offset_inline( int32_t t, int delta, Blip_Buffer* buf ) const {
			offset_resampled( t * buf->factor_ + buf->offset_, delta, buf );
		}
		void offset_inline( int32_t t, int delta ) const {
			offset_resampled( t * impl.buf->factor_ + impl.buf->offset_, delta, impl.buf );
		}
	private:
		Blip_Synth_Fast_ impl;
};

#if __GNUC__ >= 3 || _MSC_VER >= 1100
#define BLIP_RESTRICT __restrict
#else
#define BLIP_RESTRICT
#endif

// Optimized reading from Blip_Buffer, for use in custom sample output

// Begins reading from buffer. Name should be unique to the current block.
#define BLIP_READER_BEGIN( name, blip_buffer ) \
        const Blip_Buffer::buf_t_* BLIP_RESTRICT name##_reader_buf = (blip_buffer).buffer_;\
        int32_t name##_reader_accum = (blip_buffer).reader_accum_

/* Constant value to use instead of BLIP_READER_BASS(), for slightly more optimal
   code at the cost of having no bass control */

#define BLIP_READER_DEFAULT_BASS 9
#define BLIP_SAMPLE_BITS 30

/* Current sample */
#define BLIP_READER_READ( name )        (name##_reader_accum >> 14)

// Advances to next sample
#define BLIP_READER_NEXT( name, bass ) \
        (void) (name##_reader_accum += *name##_reader_buf++ - (name##_reader_accum >> (bass)))

// Ends reading samples from buffer. The number of samples read must now be removed
// using Blip_Buffer::remove_samples().
#define BLIP_READER_END( name, blip_buffer ) \
        (void) ((blip_buffer).reader_accum_ = name##_reader_accum)

// experimental
#define BLIP_READER_ADJ_( name, offset ) (name##_reader_buf += offset)

int32_t const blip_reader_idx_factor = sizeof (Blip_Buffer::buf_t_);

#define BLIP_READER_NEXT_IDX_( name, idx ) {\
        name##_reader_accum -= name##_reader_accum >> BLIP_READER_DEFAULT_BASS;\
        name##_reader_accum += name##_reader_buf [(idx)];\
}

#define BLIP_READER_NEXT_RAW_IDX_( name, idx ) {\
        name##_reader_accum -= name##_reader_accum >> BLIP_READER_DEFAULT_BASS; \
        name##_reader_accum += *(Blip_Buffer::buf_t_ const*) ((char const*) name##_reader_buf + (idx)); \
}

#if defined (_M_IX86) || defined (_M_IA64) || defined (__i486__) || \
                defined (__x86_64__) || defined (__ia64__) || defined (__i386__)
        #define BLIP_CLAMP_( in ) in < -0x8000 || 0x7FFF < in
#else
        #define BLIP_CLAMP_( in ) (int16_t) in != in
#endif

// Clamp sample to int16_t range
#define BLIP_CLAMP( sample, out )\
        { if ( BLIP_CLAMP_( (sample) ) ) (out) = ((sample) >> 24) ^ 0x7FFF; }

struct blip_buffer_state_t
{
        uint32_t offset_;
        int32_t reader_accum_;
        int32_t buf [BLIP_BUFFER_EXTRA_];
};

// End of public interface


template<int quality,int range>
INLINE void Blip_Synth<quality,range>::offset_resampled( uint32_t time, int delta, Blip_Buffer* blip_buf ) const
{
	// If this assertion fails, it means that an attempt was made to add a delta
	// at a negative time or past the end of the buffer.
	//assert( (int32_t) (time >> BLIP_BUFFER_ACCURACY) < blip_buf->buffer_size_ );

	delta *= impl.delta_factor;
	int32_t* BLIP_RESTRICT buf = blip_buf->buffer_ + (time >> BLIP_BUFFER_ACCURACY);
	int phase = (int) (time >> (BLIP_BUFFER_ACCURACY - BLIP_PHASE_BITS) & (blip_res - 1));

	int32_t left = buf [0] + delta;

	// Kind of crappy, but doing shift after multiply results in overflow.
	// Alternate way of delaying multiply by delta_factor results in worse
	// sub-sample resolution.
	int32_t right = (delta >> BLIP_PHASE_BITS) * phase;
	left  -= right;
	right += buf [1];

	buf [0] = left;
	buf [1] = right;
}

#undef BLIP_FWD
#undef BLIP_REV

template<int quality,int range>
INLINE void Blip_Synth<quality,range>::offset( int32_t t, int delta, Blip_Buffer* buf ) const
{
        offset_resampled( t * buf->factor_ + buf->offset_, delta, buf );
}

template<int quality,int range>
INLINE void Blip_Synth<quality,range>::update( int32_t t, int amp)
{
	int delta = amp - impl.last_amp;
	impl.last_amp = amp;
	offset_resampled( t * impl.buf->factor_ + impl.buf->offset_, delta, impl.buf );
}

#define SAMPLES_AVAILABLE() ((long)(offset_ >> BLIP_BUFFER_ACCURACY))

INLINE int  Blip_Buffer::length() const         { return length_; }
INLINE long Blip_Buffer::samples_avail() const  { return (long) (offset_ >> BLIP_BUFFER_ACCURACY); }
INLINE long Blip_Buffer::sample_rate() const    { return sample_rate_; }
INLINE long Blip_Buffer::clock_rate() const     { return clock_rate_; }
INLINE void Blip_Buffer::clock_rate( long cps ) { factor_ = clock_rate_factor( clock_rate_ = cps ); }

/* 1/4th of a second */
#define BLIP_DEFAULT_LENGTH 250

#define	wave_type	0x100
#define noise_type	0x200
#define mixed_type	wave_type | noise_type
#define type_index_mask	0xFF

struct channel_t {
	Blip_Buffer* center;
	Blip_Buffer* left;
	Blip_Buffer* right;
};

/* Uses three buffers (one for center) and outputs stereo sample pairs. */

#define STEREO_BUFFER_SAMPLES_AVAILABLE() ((long)(bufs_buffer[0].offset_ -  mixer_samples_read) << 1)

class Stereo_Buffer
{
	public:
		Stereo_Buffer();
		~Stereo_Buffer();
		const char * set_sample_rate( long, int msec = BLIP_DEFAULT_LENGTH);
		void clock_rate( long );
		void bass_freq( int );
		void clear();

		double real_ratio();

		channel_t channel( int ) { return chan; }
		void end_frame( int32_t );

		long samples_avail() { return (bufs_buffer [0].samples_avail() - mixer_samples_read) << 1; }
		long read_samples( int16_t*, long );
		void mixer_read_pairs( int16_t* out, int count );
		enum { bufs_size = 3 };
		typedef Blip_Buffer buf_t;
		buf_t bufs_buffer [bufs_size];
		bool immediate_removal_;
	private:
		Blip_Buffer* mixer_bufs [3];
		blargg_long mixer_samples_read;
		channel_t chan;
		long samples_avail_;

		/* Count of changes to channel configuration. Incremented whenever
		   a change is made to any of the Blip_Buffers for any channel. */

		unsigned channels_changed_count_;
		long sample_rate_;
		/* Length of buffer, in milliseconds */
		int length_;
		int channel_count_;
		/* Number of samples per output frame (1 = mono, 2 = stereo) */
		int samples_per_frame_;
		int const* channel_types_;
};


/* See Simple_Effects_Buffer (below) for a simpler interface */

class Effects_Buffer {
	public:
		// To reduce memory usage, fewer buffers can be used (with a best-fit
		// approach if there are too few), and maximum echo delay can be reduced
		Effects_Buffer( int max_bufs = 32, long echo_size = 24 * 1024L );
		virtual ~Effects_Buffer();

		struct pan_vol_t
		{
			float vol; // 0.0 = silent, 0.5 = half volume, 1.0 = normal
			float pan; // -1.0 = left, 0.0 = center, +1.0 = right
		};

		// Global configuration
		struct config_t
		{
			bool enabled; // false = disable all effects

			// Current sound is echoed at adjustable left/right delay,
			// with reduced treble and volume (feedback).
			float treble;   // 1.0 = full treble, 0.1 = very little, 0.0 = silent
			int delay [2];  // left, right delays (msec)
			float feedback; // 0.0 = no echo, 0.5 = each echo half previous, 1.0 = cacophony
			pan_vol_t side_chans [2]; // left and right side channel volume and pan
		};
		config_t& config() { return config_; }

		// Per-channel configuration. Two or more channels with matching parameters are
		// optimized to internally use the same buffer.
		struct chan_config_t : pan_vol_t
		{
			bool surround;  // if true, negates left volume to put sound in back
			bool echo;      // false = channel doesn't have any echo
		};
		chan_config_t& chan_config( int i ) { return chans [i + extra_chans].cfg; }

		// Apply any changes made to config() and chan_config()
		virtual void apply_config();
		void clear();
		const char * set_sample_rate( long samples_per_sec, int msec = BLIP_DEFAULT_LENGTH);

		/* Sets the number of channels available and optionally their types
		(type information used by Effects_Buffer) */

		const char * set_channel_count( int, int const* = 0 );
		void clock_rate( long );
		void bass_freq( int );

		// Gets indexed channel, from 0 to channel count - 1
		channel_t channel( int i);
		void end_frame( int32_t );
		long read_samples( int16_t*, long );
		long samples_avail() const { return (bufs_buffer [0].samples_avail() - mixer_samples_read) * 2; }
		enum { stereo = 2 };
		typedef blargg_long fixed_t;
		void mixer_read_pairs( int16_t* out, int count );
		bool immediate_removal_;
		long sample_rate_;
		int length_;
		int channel_count_;
		int const* channel_types_;
	protected:
		enum { extra_chans = stereo * stereo };
		void channels_changed() { channels_changed_count_++; }
	private:
		config_t config_;
		long clock_rate_;
		int bass_freq_;

		blargg_long echo_size;

		struct chan_t
		{
			fixed_t vol [stereo];
			chan_config_t cfg;
			channel_t channel;
		};
		blargg_vector<chan_t> chans;

		struct buf_t : Blip_Buffer
		{
		fixed_t vol [stereo];
		bool echo;

		void* operator new ( size_t, void* p ) { return p; }
		void operator delete ( void* ) { }

		~buf_t() { }
		};
		buf_t* bufs_buffer;
		int bufs_size;
		int bufs_max; // bufs_size <= bufs_max, to limit memory usage
		Blip_Buffer* mixer_bufs [3];
		blargg_long mixer_samples_read;

		struct {
			long delay [stereo];
			fixed_t treble;
			fixed_t feedback;
			fixed_t low_pass [stereo];
		} s;

		blargg_vector<fixed_t> echo;
		blargg_long echo_pos;

		bool no_effects;
		bool no_echo;

		void mix_effects( int16_t* out, int pair_count );
		const char * new_bufs( int size );
		//from Multi_Buffer
		unsigned channels_changed_count_;
		int samples_per_frame_;
};

/* Simpler interface and lower memory usage */
class Simple_Effects_Buffer : public Effects_Buffer
{
	public:
	Simple_Effects_Buffer();
	struct config_t
	{
		bool enabled;   // false = disable all effects
		float echo;     // 0.0 = none, 1.0 = lots
		float stereo;   // 0.0 = channels in center, 1.0 = channels on left/right
		bool surround;  // true = put some channels in back
	};
	config_t& config() { return config_; }

	// Apply any changes made to config()
	void apply_config();
	private:
	config_t config_;
	void chan_config(); // hide
};

#endif
