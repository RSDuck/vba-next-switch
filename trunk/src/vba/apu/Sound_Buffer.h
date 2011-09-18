// Multi-channel sound buffer interface, and basic mono and stereo buffers

// Blip_Buffer 0.4.1
#ifndef MULTI_BUFFER_H
#define MULTI_BUFFER_H

#include "../System.h"

#include "blargg_common.h"

/* BLIP BUFFER */

// internal
#include <limits.h>
#if INT_MAX < 0x7FFFFFFF || LONG_MAX == 0x7FFFFFFF
        typedef long blip_long;
        typedef unsigned long blip_ulong;
#else
        typedef int blip_long;
        typedef unsigned blip_ulong;
#endif

// Time unit at source clock rate
typedef blip_long blip_time_t;

// Output samples are 16-bit signed, with a range of -32768 to 32767
typedef short blip_sample_t;
enum { blip_sample_max = 32767 };

struct blip_buffer_state_t;

class Blip_Buffer
{
	public:
	typedef const char* blargg_err_t;
	blip_ulong non_silent() const;

	// Sets output sample rate and buffer length in milliseconds (1/1000 sec, defaults
	// to 1/4 second) and clears buffer. If there isn't enough memory, leaves buffer
	// untouched and returns "Out of memory", otherwise returns NULL.
	blargg_err_t set_sample_rate( long samples_per_sec, int msec_length = 1000 / 4 );

	// Sets number of source time units per second
	void clock_rate( long clocks_per_sec );

	// Ends current time frame of specified duration and makes its samples available
	// (along with any still-unread samples) for reading with read_samples(). Begins
	// a new time frame at the end of the current frame.
	void end_frame( blip_time_t time );

	// Reads at most 'max_samples' out of buffer into 'dest', removing them from
	// the buffer. Returns number of samples actually read and removed. If stereo is
	// true, increments 'dest' one extra time after writing each sample, to allow
	// easy interleving of two channels into a stereo output buffer.
	long read_samples( blip_sample_t* dest, long count);

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
	//long count_samples( blip_time_t duration ) const;

	// Mixes in 'count' samples from 'buf_in'
	//void mix_samples( blip_sample_t const* buf_in, long count );


	// Signals that sound has been added to buffer. Could be done automatically in
	// Blip_Synth, but that would affect performance more, as you can arrange that
	// this is called only once per time frame rather than for every delta.
	void set_modified() { modified_ = this; }

	// not documented yet
	blip_ulong unsettled() const;
	Blip_Buffer* clear_modified() { Blip_Buffer* b = modified_; modified_ = 0; return b; }
	void remove_silence( long count );
	typedef blip_ulong blip_resampled_time_t;
	blip_resampled_time_t resampled_time( blip_time_t t ) const { return t * factor_ + offset_; }
	blip_resampled_time_t clock_rate_factor( long clock_rate ) const;
	public:
	Blip_Buffer();
	~Blip_Buffer();

	// Deprecated
	typedef blip_resampled_time_t resampled_time_t;
	blargg_err_t sample_rate( long r ) { return set_sample_rate( r ); }
	blargg_err_t sample_rate( long r, int msec ) { return set_sample_rate( r, msec ); }
	private:
	// noncopyable
	Blip_Buffer( const Blip_Buffer& );
	Blip_Buffer& operator = ( const Blip_Buffer& );
	public:
	typedef blip_long buf_t_;
	blip_ulong factor_;
	blip_resampled_time_t offset_;
	buf_t_* buffer_;
	blip_long buffer_size_;
	blip_long reader_accum_;
	int bass_shift_;
	Blip_Buffer* modified_; // non-zero = true (more optimal than using bool, heh)
	blip_long last_non_silence;
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
typedef blip_ulong blip_resampled_time_t;
int const blip_widest_impulse_ = 16;
int const blip_buffer_extra_ = blip_widest_impulse_ + 2;
int const blip_res = 1 << BLIP_PHASE_BITS;
class blip_eq_t;

class Blip_Synth_Fast_
{
	public:
	Blip_Buffer* buf;
	int last_amp;
	int delta_factor;

	void volume_unit( double );
	Blip_Synth_Fast_();
	void treble_eq( blip_eq_t const& ) { }
};

class Blip_Synth_ {
	public:
		Blip_Buffer* buf;
		int last_amp;
		int delta_factor;

		void volume_unit( double );
		Blip_Synth_( short* impulses, int width );
		void treble_eq( blip_eq_t const& );
	private:
		double volume_unit_;
		short* const impulses;
		int const width;
		blip_long kernel_unit;
		int impulses_size() const { return blip_res / 2 * width + 1; }
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

		// Configures low-pass filter (see blip_buffer.txt)
		void treble_eq( blip_eq_t const& eq )       { impl.treble_eq( eq ); }

		void output( Blip_Buffer* b )               { impl.buf = b; impl.last_amp = 0; }

		// Updates amplitude of waveform at given time. Using this requires a separate
		// Blip_Synth for each waveform.
		void update( blip_time_t time, int amplitude );

		// Low-level interface

		// Adds an amplitude transition of specified delta, optionally into specified buffer
		// rather than the one set with output(). Delta can be positive or negative.
		// The actual change in amplitude is delta * (volume / range)
		void offset( blip_time_t, int delta, Blip_Buffer* ) const;
		void offset( blip_time_t t, int delta ) const { offset( t, delta, impl.buf ); }

		// Works directly in terms of fractional output samples. Contact author for more info.
		void offset_resampled( blip_resampled_time_t, int delta, Blip_Buffer* ) const;

		// Same as offset(), except code is inlined for higher performance
		void offset_inline( blip_time_t t, int delta, Blip_Buffer* buf ) const {
			offset_resampled( t * buf->factor_ + buf->offset_, delta, buf );
		}
		void offset_inline( blip_time_t t, int delta ) const {
			offset_resampled( t * impl.buf->factor_ + impl.buf->offset_, delta, impl.buf );
		}
	private:
		Blip_Synth_Fast_ impl;
};

// Low-pass equalization parameters
class blip_eq_t {
	public:
		// Logarithmic rolloff to treble dB at half sampling rate. Negative values reduce
		// treble, small positive values (0 to 5.0) increase treble.
		blip_eq_t( double treble_db = 0 );

		// See blip_buffer.txt
		blip_eq_t( double treble, long rolloff_freq, long sample_rate, long cutoff_freq = 0 );

	private:
		double treble;
		long rolloff_freq;
		long sample_rate;
		long cutoff_freq;
		void generate( float* out, int count ) const;
		friend class Blip_Synth_;
};

int const blip_sample_bits = 30;

#if __GNUC__ >= 3 || _MSC_VER >= 1100
#define BLIP_RESTRICT __restrict
#else
#define BLIP_RESTRICT
#endif

// Optimized reading from Blip_Buffer, for use in custom sample output

// Begins reading from buffer. Name should be unique to the current block.
#define BLIP_READER_BEGIN( name, blip_buffer ) \
        const Blip_Buffer::buf_t_* BLIP_RESTRICT name##_reader_buf = (blip_buffer).buffer_;\
        blip_long name##_reader_accum = (blip_buffer).reader_accum_

// Gets value to pass to BLIP_READER_NEXT()
#define BLIP_READER_BASS( blip_buffer ) ((blip_buffer).bass_shift_)

// Constant value to use instead of BLIP_READER_BASS(), for slightly more optimal
// code at the cost of having no bass control
int const blip_reader_default_bass = 9;

// Current sample
#define BLIP_READER_READ( name )        (name##_reader_accum >> (blip_sample_bits - 16))

// Current raw sample in full internal resolution
#define BLIP_READER_READ_RAW( name )    (name##_reader_accum)

// Advances to next sample
#define BLIP_READER_NEXT( name, bass ) \
        (void) (name##_reader_accum += *name##_reader_buf++ - (name##_reader_accum >> (bass)))

// Ends reading samples from buffer. The number of samples read must now be removed
// using Blip_Buffer::remove_samples().
#define BLIP_READER_END( name, blip_buffer ) \
        (void) ((blip_buffer).reader_accum_ = name##_reader_accum)

// experimental
#define BLIP_READER_ADJ_( name, offset ) (name##_reader_buf += offset)

blip_long const blip_reader_idx_factor = sizeof (Blip_Buffer::buf_t_);

#define BLIP_READER_NEXT_IDX_( name, bass, idx ) {\
        name##_reader_accum -= name##_reader_accum >> (bass);\
        name##_reader_accum += name##_reader_buf [(idx)];\
}

#define BLIP_READER_NEXT_RAW_IDX_( name, bass, idx ) {\
        name##_reader_accum -= name##_reader_accum >> (bass);\
        name##_reader_accum +=\
                        *(Blip_Buffer::buf_t_ const*) ((char const*) name##_reader_buf + (idx));\
}

const int blip_low_quality  = blip_med_quality;
const int blip_best_quality = blip_high_quality;


#if defined (_M_IX86) || defined (_M_IA64) || defined (__i486__) || \
                defined (__x86_64__) || defined (__ia64__) || defined (__i386__)
        #define BLIP_CLAMP_( in ) in < -0x8000 || 0x7FFF < in
#else
        #define BLIP_CLAMP_( in ) (blip_sample_t) in != in
#endif

// Clamp sample to blip_sample_t range
#define BLIP_CLAMP( sample, out )\
        { if ( BLIP_CLAMP_( (sample) ) ) (out) = ((sample) >> 24) ^ 0x7FFF; }

struct blip_buffer_state_t
{
        blip_resampled_time_t offset_;
        blip_long reader_accum_;
        blip_long buf [blip_buffer_extra_];
};

// End of public interface


template<int quality,int range>
inline void Blip_Synth<quality,range>::offset_resampled( blip_resampled_time_t time, int delta, Blip_Buffer* blip_buf ) const
{
	// If this assertion fails, it means that an attempt was made to add a delta
	// at a negative time or past the end of the buffer.
	//assert( (blip_long) (time >> BLIP_BUFFER_ACCURACY) < blip_buf->buffer_size_ );

	delta *= impl.delta_factor;
	blip_long* BLIP_RESTRICT buf = blip_buf->buffer_ + (time >> BLIP_BUFFER_ACCURACY);
	int phase = (int) (time >> (BLIP_BUFFER_ACCURACY - BLIP_PHASE_BITS) & (blip_res - 1));

	blip_long left = buf [0] + delta;

	// Kind of crappy, but doing shift after multiply results in overflow.
	// Alternate way of delaying multiply by delta_factor results in worse
	// sub-sample resolution.
	blip_long right = (delta >> BLIP_PHASE_BITS) * phase;
	left  -= right;
	right += buf [1];

	buf [0] = left;
	buf [1] = right;
}

#undef BLIP_FWD
#undef BLIP_REV

template<int quality,int range>
inline void Blip_Synth<quality,range>::offset( blip_time_t t, int delta, Blip_Buffer* buf ) const
{
        offset_resampled( t * buf->factor_ + buf->offset_, delta, buf );
}

template<int quality,int range>
inline void Blip_Synth<quality,range>::update( blip_time_t t, int amp)
{
	int delta = amp - impl.last_amp;
	impl.last_amp = amp;
	offset_resampled( t * impl.buf->factor_ + impl.buf->offset_, delta, impl.buf );
}

inline blip_eq_t::blip_eq_t( double t ) : treble( t ), rolloff_freq( 0 ), sample_rate( 44100 ), cutoff_freq( 0 ) { }
inline blip_eq_t::blip_eq_t( double t, long rf, long sr, long cf ) : treble( t ), rolloff_freq( rf ), sample_rate( sr ), cutoff_freq( cf ) { }

inline int  Blip_Buffer::length() const         { return length_; }
inline long Blip_Buffer::samples_avail() const  { return (long) (offset_ >> BLIP_BUFFER_ACCURACY); }
inline long Blip_Buffer::sample_rate() const    { return sample_rate_; }
inline long Blip_Buffer::clock_rate() const     { return clock_rate_; }
inline void Blip_Buffer::clock_rate( long cps ) { factor_ = clock_rate_factor( clock_rate_ = cps ); }

int const blip_max_length = 0;
int const blip_default_length = 250; // 1/4 second

/* MULTI BUFFER */

// Interface to one or more Blip_Buffers mapped to one or more channels
// consisting of left, center, and right buffers.
class Multi_Buffer {
	public:
		Multi_Buffer( int samples_per_frame );
		virtual ~Multi_Buffer() { }

		// Sets the number of channels available and optionally their types
		// (type information used by Effects_Buffer)
		enum { type_index_mask = 0xFF };
		enum { wave_type = 0x100, noise_type = 0x200, mixed_type = wave_type | noise_type };
		virtual blargg_err_t set_channel_count( int, int const* types = 0 );
		int channel_count() const { return channel_count_; }

		// Gets indexed channel, from 0 to channel count - 1
		struct channel_t {
			Blip_Buffer* center;
			Blip_Buffer* left;
			Blip_Buffer* right;
		};
		virtual channel_t channel( int index ) BLARGG_PURE( ; )

			// See Blip_Buffer.h
			virtual blargg_err_t set_sample_rate( long rate, int msec = blip_default_length ) BLARGG_PURE( ; )
			virtual void clock_rate( long ) BLARGG_PURE( { } )
			virtual void bass_freq( int ) BLARGG_PURE( { } )
			virtual void clear() BLARGG_PURE( { } )
			long sample_rate() const;

		// Length of buffer, in milliseconds
		int length() const;

		// See Blip_Buffer.h
		virtual void end_frame( blip_time_t ) BLARGG_PURE( { } )

			// Number of samples per output frame (1 = mono, 2 = stereo)
			int samples_per_frame() const;

		// Count of changes to channel configuration. Incremented whenever
		// a change is made to any of the Blip_Buffers for any channel.
		unsigned channels_changed_count() { return channels_changed_count_; }

		// See Blip_Buffer.h
		virtual long read_samples( blip_sample_t*, long ) BLARGG_PURE( { return 0; } )
			//virtual long samples_avail() const BLARGG_PURE( { return 0; } )

	public:
			BLARGG_DISABLE_NOTHROW
				void disable_immediate_removal() { immediate_removal_ = false; }
	protected:
			bool immediate_removal() const { return immediate_removal_; }
			int const* channel_types() const { return channel_types_; }
			void channels_changed() { channels_changed_count_++; }
	private:
			// noncopyable
			Multi_Buffer( const Multi_Buffer& );
			Multi_Buffer& operator = ( const Multi_Buffer& );

			unsigned channels_changed_count_;
			long sample_rate_;
			int length_;
			int channel_count_;
			int samples_per_frame_;
			int const* channel_types_;
			bool immediate_removal_;
};

// Uses three buffers (one for center) and outputs stereo sample pairs.
class Stereo_Buffer : public Multi_Buffer {
	public:
		Stereo_Buffer();
		~Stereo_Buffer();
		blargg_err_t set_sample_rate( long, int msec = blip_default_length );
		void clock_rate( long );
		void bass_freq( int );
		void clear();
		channel_t channel( int ) { return chan; }
		void end_frame( blip_time_t );

		long samples_avail() { return (bufs_buffer [0].samples_avail() - mixer_samples_read) << 1; }
		long read_samples( blip_sample_t*, long );
		void mixer_read_pairs( blip_sample_t* out, int count );
		enum { bufs_size = 3 };
		typedef Blip_Buffer buf_t;
		buf_t bufs_buffer [bufs_size];
	private:
		Blip_Buffer* mixer_bufs [3];
		blargg_long mixer_samples_read;
		channel_t chan;
		long samples_avail_;
};

inline blargg_err_t Multi_Buffer::set_sample_rate( long rate, int msec )
{
	sample_rate_ = rate;
	length_ = msec;
	return 0;
}

inline int Multi_Buffer::samples_per_frame() const { return samples_per_frame_; }

inline long Multi_Buffer::sample_rate() const { return sample_rate_; }

inline int Multi_Buffer::length() const { return length_; }

inline blargg_err_t Multi_Buffer::set_channel_count( int n, int const* types )
{
        channel_count_ = n;
        channel_types_ = types;
        return 0;
}

// See Simple_Effects_Buffer (below) for a simpler interface

class Effects_Buffer : public Multi_Buffer {
	public:
		// To reduce memory usage, fewer buffers can be used (with a best-fit
		// approach if there are too few), and maximum echo delay can be reduced
		Effects_Buffer( int max_bufs = 32, long echo_size = 24 * 1024L );
		~Effects_Buffer();

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
		blargg_err_t set_sample_rate( long samples_per_sec, int msec = blip_default_length );
		blargg_err_t set_channel_count( int, int const* = 0 );
		void clock_rate( long );
		void bass_freq( int );
		channel_t channel( int );
		void end_frame( blip_time_t );
		long read_samples( blip_sample_t*, long );
		long samples_avail() const { return (bufs_buffer [0].samples_avail() - mixer_samples_read) * 2; }
		enum { stereo = 2 };
		typedef blargg_long fixed_t;
		void mixer_read_pairs( blip_sample_t* out, int count );
	protected:
		enum { extra_chans = stereo * stereo };
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

		void mix_effects( blip_sample_t* out, int pair_count );
		blargg_err_t new_bufs( int size );
};

// Simpler interface and lower memory usage
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
