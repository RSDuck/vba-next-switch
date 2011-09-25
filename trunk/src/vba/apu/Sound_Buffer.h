// Multi-channel sound buffer interface, and basic mono and stereo buffers

// Blip_Buffer 0.4.1
#ifndef MULTI_BUFFER_H
#define MULTI_BUFFER_H

#include <math.h>
#include "../System.h"

#include "blargg_common.h"

#ifdef HAVE_CONFIG_H
        #include "config.h"
#endif

/* BLIP BUFFER */

// Number of bits in resample ratio fraction. Higher values give a more accurate ratio
// but reduce maximum buffer size.
#define BLIP_BUFFER_ACCURACY 16

// Output samples are 16-bit signed, with a range of -32768 to 32767
#define BLIP_SAMPLE_MAX 32767
#define STEREO 2
#define STEREO_SHIFT 1
#define EXTRA_CHANS 4

// Number bits in phase offset. Fewer than 6 bits (64 phase offsets) results in
// noticeable broadband noise when synthesizing high frequency square waves.
// Affects size of Blip_Synth objects since they store the waveform directly.
#define BLIP_PHASE_BITS 8

//blip_res = 1 << BLIP_PHASE_BITS
#define BLIP_RES 256

#define BLIP_BUFFER_END_FRAME(blip, t) blip.offset_ += t * blip.factor_

// Number of samples available for reading with read_samples()
#define BLIP_BUFFER_SAMPLES_AVAIL() (offset_ >> BLIP_BUFFER_ACCURACY)
#define BLIP_BUFFER_SAMPLES_AVAIL_INST(blip) (blip.offset_ >> BLIP_BUFFER_ACCURACY)

struct blip_buffer_state_t;

class Blip_Buffer
{
	public:
	Blip_Buffer();
	~Blip_Buffer();

	// Sets output sample rate and buffer length in milliseconds (1/1000 sec, defaults
	// to 1/4 second) and clears buffer. If there isn't enough memory, leaves buffer
	// untouched and returns -1, otherwise returns 0.
	int32_t set_sample_rate(int32_t samples_per_sec, int msec_length = 1000 / 4);

	// Sets number of source time units per second
	void clock_rate(int32_t clocks_per_sec);

	// Reads at most 'max_samples' out of buffer into 'dest', removing them from
	// the buffer. Returns number of samples actually read and removed. If stereo is
	// true, increments 'dest' one extra time after writing each sample, to allow
	// easy interleving of two channels into a stereo output buffer.
	int32_t read_samples( int16_t* dest, int32_t count);

	// Removes all available samples and clear buffer to silence.
	void clear(void);

	// Clear out any samples waiting rather than entire buffer
	void clear_false(void);


	// Removes 'count' samples from those waiting to be read
	void remove_samples(int32_t count);

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

	uint32_t factor_;
	uint32_t offset_;
	int32_t * buffer_;
	int32_t buffer_size_;
	int32_t reader_accum_;
	int bass_shift_;
	// Current output sample rate
	int32_t sample_rate_;
	// Number of source time units per second
	int32_t clock_rate_;
	// Length of buffer in milliseconds
	int length_;
	private:
	// noncopyable
	Blip_Buffer( const Blip_Buffer& );
	Blip_Buffer& operator = ( const Blip_Buffer& );
	int bass_freq_;
};

// Internal
#define BLIP_BUFFER_EXTRA_ 18

#ifdef USE_SOUND_FILTERING
class blip_eq_t;
#endif

// BLIP_SYNTH

#define BLIP_SAMPLE_BITS 30

// Sets overall volume of waveform
#define BLIP_SYNTH_VOLUME_UNIT(blip_synth, new_unit) blip_synth.delta_factor = int (new_unit * (1L << BLIP_SAMPLE_BITS) + 0.5);

#if __GNUC__ >= 3 || _MSC_VER >= 1100
#define BLIP_RESTRICT __restrict
#else
#define BLIP_RESTRICT
#endif

// Range specifies the greatest expected change in amplitude. Calculate it
// by finding the difference between the maximum and minimum expected
// amplitudes (max - min).
typedef struct Blip_Synth
{
	int32_t last_amp;
	int32_t delta_factor;
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


// Optimized reading from Blip_Buffer, for use in custom sample output

// Begins reading from buffer. Name should be unique to the current block.
#define BLIP_READER_BEGIN( name, blip_buffer ) \
        const int32_t * BLIP_RESTRICT name##_reader_buf = (blip_buffer).buffer_;\
        int32_t name##_reader_accum = (blip_buffer).reader_accum_

// Gets value to pass to BLIP_READER_NEXT()
#define BLIP_READER_BASS( blip_buffer ) ((blip_buffer).bass_shift_)

// Constant value to use instead of BLIP_READER_BASS(), for slightly more optimal
// code at the cost of having no bass control
#define BLIP_READER_DEFAULT_BASS 9

// Current sample
#define BLIP_READER_READ( name )        (name##_reader_accum >> (BLIP_SAMPLE_BITS - 16))

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
        int32_t buf[BLIP_BUFFER_EXTRA_];
};

#undef BLIP_FWD
#undef BLIP_REV


inline void Blip_Buffer::clock_rate(int32_t cps )
{
	clock_rate_ = cps;
	float ratio = (float) sample_rate_ / clock_rate_;
	int32_t factor = (int32_t) floor( ratio * (1L << BLIP_BUFFER_ACCURACY) + 0.5);
	factor_ = (uint32_t)factor;
}

#define BLIP_MAX_LENGTH	0

// 1/4th of a second
#define BLIP_DEFAULT_LENGTH 250

#define	WAVE_TYPE	0x100
#define NOISE_TYPE	0x200
#define MIXED_TYPE	WAVE_TYPE | NOISE_TYPE
#define TYPE_INDEX_MASK	0xFF

typedef struct channel_t {
	Blip_Buffer* center;
	Blip_Buffer* left;
	Blip_Buffer* right;
};

#define BUFFERS_SIZE 3

#define STEREO_BUFFER_END_FRAME(stereo_buf, time) \
        for ( int i = BUFFERS_SIZE; --i >= 0; ) \
		BLIP_BUFFER_END_FRAME(stereo_buf->bufs_buffer[i], time);

// Uses three buffers (one for center) and outputs stereo sample pairs.
class Stereo_Buffer {
	public:
		Stereo_Buffer();
		~Stereo_Buffer();
		int32_t set_sample_rate(int32_t, int msec = BLIP_DEFAULT_LENGTH );
		void clock_rate(int32_t);
		void clear();
		int32_t samples_avail() { return (BLIP_BUFFER_SAMPLES_AVAIL_INST(bufs_buffer [0]) - mixer_samples_read) << 1; }
		int32_t read_samples( int16_t*, int32_t);
		void mixer_read_pairs( int16_t* out, int count );
		Blip_Buffer bufs_buffer[BUFFERS_SIZE];
		int32_t sample_rate_;
	private:
		Blip_Buffer* mixer_bufs [3];
		int32_t mixer_samples_read;
		channel_t chan;
		int32_t samples_avail_;

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
		Effects_Buffer( int max_bufs = 32, int32_t echo_size = 24 * 1024L );
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
		chan_config_t& chan_config( int i ) { return chans [i + EXTRA_CHANS].cfg; }

		// Apply any changes made to config() and chan_config()
		virtual void apply_config();
		void clear();
		int32_t set_sample_rate(int32_t samples_per_sec, int msec = BLIP_DEFAULT_LENGTH);

		// Sets the number of channels available and optionally their types
		// (type information used by Effects_Buffer)
		int32_t set_channel_count( int, int const* = 0 );
		void clock_rate(int32_t);
		void bass_freq( int );

		// Gets indexed channel, from 0 to channel count - 1
		channel_t channel( int i);
		void end_frame( int32_t );
		int32_t read_samples( int16_t*, int32_t);
		int32_t samples_avail() const { return (BLIP_BUFFER_SAMPLES_AVAIL_INST(bufs_buffer [0]) - mixer_samples_read) * 2; }
		void mixer_read_pairs( int16_t* out, int count );
		int32_t sample_rate_;
		int length_;
		int channel_count_;
		int const* channel_types_;
	protected:
		void channels_changed() { channels_changed_count_++; }
	private:
		config_t config_;
		int32_t clock_rate_;
		int bass_freq_;

		int32_t echo_size;

		struct chan_t
		{
			int32_t vol[STEREO];
			chan_config_t cfg;
			channel_t channel;
		};
		blargg_vector<chan_t> chans;

		struct buf_t : Blip_Buffer
		{
			int32_t vol[STEREO];
			bool echo;

			void* operator new ( size_t, void* p ) { return p; }
			void operator delete ( void* ) { }

			~buf_t() { }
		};
		buf_t* bufs_buffer;
		int bufs_size;
		int bufs_max; // bufs_size <= bufs_max, to limit memory usage
		Blip_Buffer* mixer_bufs [3];
		int32_t mixer_samples_read;

		struct {
			int32_t delay[STEREO];
			int32_t treble;
			int32_t feedback;
			int32_t low_pass[STEREO];
		} s_struct;

		blargg_vector<int32_t> echo;
		int32_t echo_pos;

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

extern void offset_resampled(int delta_factor, uint32_t time, int delta, Blip_Buffer* blip_buf );
