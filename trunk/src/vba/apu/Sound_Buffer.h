// Multi-channel sound buffer interface, and basic mono and stereo buffers

// Blip_Buffer 0.4.1
#ifndef MULTI_BUFFER_H
#define MULTI_BUFFER_H

#include "../System.h"

#include "blargg_common.h"

/* BLIP BUFFER */

// Output samples are 16-bit signed, with a range of -32768 to 32767
#define blip_sample_max	32767

struct blip_buffer_state_t;

class Blip_Buffer
{
	public:
	Blip_Buffer();
	~Blip_Buffer();
	#ifndef FASTER_SOUND_HACK_NON_SILENCE
	uint32_t non_silent() const;
	#endif

	// Sets output sample rate and buffer length in milliseconds (1/1000 sec, defaults
	// to 1/4 second) and clears buffer. If there isn't enough memory, leaves buffer
	// untouched and returns -1, otherwise returns 0.
	int32_t set_sample_rate(long samples_per_sec, int msec_length = 1000 / 4);

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

	// Removes all available samples and clear buffer to silence. If 'entire_buffer' is
	// false, just clears out any samples waiting rather than the entire buffer.
	void clear( int entire_buffer = 1 );

	// Number of samples available for reading with read_samples()
	long samples_avail() const;

	// Removes 'count' samples from those waiting to be read
	void remove_samples( long count );

	// Sets frequency high-pass filter frequency, where higher values reduce bass more
	void bass_freq( int frequency );

	// Experimental features

	// Saves state, including high-pass filter and tails of last deltas.
	// All samples must have been read from buffer before calling this.
	void save_state( blip_buffer_state_t* out );

	// Loads state. State must have been saved from Blip_Buffer with same
	// settings during same run of program. States can NOT be stored on disk.
	// Clears buffer before loading state.
	void load_state( blip_buffer_state_t const& in );

	// Signals that sound has been added to buffer. Could be done automatically in
	// Blip_Synth, but that would affect performance more, as you can arrange that
	// this is called only once per time frame rather than for every delta.
	void set_modified() { modified_ = this; }

	// not documented yet
	Blip_Buffer* clear_modified() { Blip_Buffer* b = modified_; modified_ = 0; return b; }
	void remove_silence( long count );
	uint32_t resampled_time( int32_t t ) const { return t * factor_ + offset_; }
	uint32_t clock_rate_factor( long clock_rate ) const;
	uint32_t factor_;
	uint32_t offset_;
	int32_t * buffer_;
	int32_t buffer_size_;
	int32_t reader_accum_;
	int bass_shift_;
	Blip_Buffer* modified_; // non-zero = true (more optimal than using bool, heh)
	int32_t last_non_silence;
	// Current output sample rate
	long sample_rate_;
	// Number of source time units per second
	long clock_rate_;
	// Length of buffer in milliseconds
	int length_;
	private:
	// noncopyable
	Blip_Buffer( const Blip_Buffer& );
	Blip_Buffer& operator = ( const Blip_Buffer& );
	int bass_freq_;
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
#define blip_widest_impulse_ 16
int const blip_buffer_extra_ = blip_widest_impulse_ + 2;
int const blip_res = 1 << BLIP_PHASE_BITS;

#ifdef USE_SOUND_FILTERING
class blip_eq_t;
#endif

// Quality level, better = slower. In general, use blip_good_quality.
#define blip_med_quality	8
#define blip_good_quality	12
#define blip_high_quality	16

#define blip_sample_bits 30

// Range specifies the greatest expected change in amplitude. Calculate it
// by finding the difference between the maximum and minimum expected
// amplitudes (max - min).
template<int quality>
class Blip_Synth {
	public:
		Blip_Synth() { impl_buf = 0; last_amp = 0; delta_factor = 0; }
		Blip_Buffer* impl_buf;
		int last_amp;
		int delta_factor;

		// Sets overall volume of waveform
		void volume_unit(double new_unit) { delta_factor = int (new_unit * (1L << blip_sample_bits) + 0.5); };
		void output( Blip_Buffer* b )               { impl_buf = b; last_amp = 0; }

		// Updates amplitude of waveform at given time. Using this requires a separate
		// Blip_Synth for each waveform.
		void update( int32_t time, int amplitude );

		// Low-level interface

		// Adds an amplitude transition of specified delta, optionally into specified buffer
		// rather than the one set with output(). Delta can be positive or negative.
		// The actual change in amplitude is delta * (volume / range)

		// Works directly in terms of fractional output samples. Contact author for more info.
		void offset_resampled( uint32_t, int delta, Blip_Buffer* ) const;
};

#ifdef USE_SOUND_FILTERING
// Low-pass equalization parameters
class blip_eq_t {
	public:
		// Logarithmic rolloff to treble dB at half sampling rate. Negative values reduce
		// treble, small positive values (0 to 5.0) increase treble.
		blip_eq_t( double treble_db = 0 );
		blip_eq_t( double treble, long rolloff_freq, long sample_rate, long cutoff_freq = 0 );
	private:
		double treble;
		long rolloff_freq;
		long sample_rate;
		long cutoff_freq;
		void generate( float* out, int count ) const;
};

inline blip_eq_t::blip_eq_t( double t ) : treble( t ), rolloff_freq( 0 ), sample_rate( 44100 ), cutoff_freq( 0 ) { }
inline blip_eq_t::blip_eq_t( double t, long rf, long sr, long cf ) : treble( t ), rolloff_freq( rf ), sample_rate( sr ), cutoff_freq( cf ) { }
#endif

#if __GNUC__ >= 3 || _MSC_VER >= 1100
#define BLIP_RESTRICT __restrict
#else
#define BLIP_RESTRICT
#endif

// Optimized reading from Blip_Buffer, for use in custom sample output

// Begins reading from buffer. Name should be unique to the current block.
#define BLIP_READER_BEGIN( name, blip_buffer ) \
        const int32_t * BLIP_RESTRICT name##_reader_buf = (blip_buffer).buffer_;\
        int32_t name##_reader_accum = (blip_buffer).reader_accum_

// Gets value to pass to BLIP_READER_NEXT()
#define BLIP_READER_BASS( blip_buffer ) ((blip_buffer).bass_shift_)

// Constant value to use instead of BLIP_READER_BASS(), for slightly more optimal
// code at the cost of having no bass control
#define blip_reader_default_bass 9

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

int32_t const blip_reader_idx_factor = sizeof (int32_t);

#define BLIP_READER_NEXT_IDX_( name, bass, idx ) {\
        name##_reader_accum -= name##_reader_accum >> (bass);\
        name##_reader_accum += name##_reader_buf [(idx)];\
}

#define BLIP_READER_NEXT_RAW_IDX_( name, bass, idx ) {\
        name##_reader_accum -= name##_reader_accum >> (bass);\
        name##_reader_accum += *(int32_t const*) ((char const*) name##_reader_buf + (idx));\
}

const int blip_low_quality  = blip_med_quality;
const int blip_best_quality = blip_high_quality;


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
        int32_t buf [blip_buffer_extra_];
};

template<int quality>
inline void Blip_Synth<quality>::offset_resampled( uint32_t time, int delta, Blip_Buffer* blip_buf ) const
{
	delta *= delta_factor;
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


inline long Blip_Buffer::samples_avail() const  { return (long) (offset_ >> BLIP_BUFFER_ACCURACY); }
inline void Blip_Buffer::clock_rate( long cps ) { factor_ = clock_rate_factor( clock_rate_ = cps ); }

#define blip_max_length	0

// 1/4th of a second
#define blip_default_length 250

#define	wave_type	0x100
#define noise_type	0x200
#define mixed_type	wave_type | noise_type
#define type_index_mask	0xFF

typedef struct channel_t {
	Blip_Buffer* center;
	Blip_Buffer* left;
	Blip_Buffer* right;
};

// Uses three buffers (one for center) and outputs stereo sample pairs.
class Stereo_Buffer {
	public:
		Stereo_Buffer();
		~Stereo_Buffer();
		int32_t set_sample_rate( long, int msec = blip_default_length );
		void clock_rate( long );
		void clear();

		// Gets indexed channel, from 0 to channel count - 1
		channel_t channel( int ) { return chan; }
		void end_frame( int32_t );

		long samples_avail() { return (bufs_buffer [0].samples_avail() - mixer_samples_read) << 1; }
		long read_samples( int16_t*, long );
		void mixer_read_pairs( int16_t* out, int count );
		enum { bufs_size = 3 };
		Blip_Buffer bufs_buffer [bufs_size];
		bool immediate_removal_;
		long sample_rate_;
	private:
		Blip_Buffer* mixer_bufs [3];
		blargg_long mixer_samples_read;
		channel_t chan;
		long samples_avail_;

		//from Multi_Buffer

		// Count of changes to channel configuration. Incremented whenever
		// a change is made to any of the Blip_Buffers for any channel.
		unsigned channels_changed_count_;
		// Length of buffer, in milliseconds
		int length_;
		int channel_count_;
		// Number of samples per output frame (1 = mono, 2 = stereo)
		int samples_per_frame_;
		int const* channel_types_;
};

typedef struct pan_vol_t
{
	float vol; // 0.0 = silent, 0.5 = half volume, 1.0 = normal
	float pan; // -1.0 = left, 0.0 = center, +1.0 = right
};

// See Simple_Effects_Buffer (below) for a simpler interface

class Effects_Buffer {
	public:
		// To reduce memory usage, fewer buffers can be used (with a best-fit
		// approach if there are too few), and maximum echo delay can be reduced
		Effects_Buffer( int max_bufs = 32, long echo_size = 24 * 1024L );
		virtual ~Effects_Buffer();

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
		struct chan_config_t
		{
			float vol; // 0.0 = silent, 0.5 = half volume, 1.0 = normal
			float pan; // -1.0 = left, 0.0 = center, +1.0 = right
			bool surround;  // if true, negates left volume to put sound in back
			bool echo;      // false = channel doesn't have any echo
		};
		chan_config_t& chan_config( int i ) { return chans [i + extra_chans].cfg; }

		// Apply any changes made to config() and chan_config()
		virtual void apply_config();
		void clear();
		int32_t set_sample_rate(long samples_per_sec, int msec = blip_default_length);

		// Sets the number of channels available and optionally their types
		// (type information used by Effects_Buffer)
		int32_t set_channel_count( int, int const* = 0 );
		void clock_rate( long );
		void bass_freq( int );

		// Gets indexed channel, from 0 to channel count - 1
		channel_t channel( int i);
		void end_frame( int32_t );
		long read_samples( int16_t*, long );
		long samples_avail() const { return (bufs_buffer [0].samples_avail() - mixer_samples_read) * 2; }
		enum { stereo = 2 };
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
			blargg_long vol [stereo];
			chan_config_t cfg;
			channel_t channel;
		};
		blargg_vector<chan_t> chans;

		struct buf_t : Blip_Buffer
		{
			blargg_long vol [stereo];
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
			blargg_long treble;
			blargg_long feedback;
			blargg_long low_pass [stereo];
		} s_struct;

		blargg_vector<blargg_long> echo;
		blargg_long echo_pos;

		bool no_effects;
		bool no_echo;

		void mix_effects( int16_t* out, int pair_count );
		int32_t new_bufs( int size );
		//from Multi_Buffer
		unsigned channels_changed_count_;
		int samples_per_frame_;
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
