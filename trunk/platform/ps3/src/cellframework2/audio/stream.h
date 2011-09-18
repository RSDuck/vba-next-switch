#ifndef __CELL_STREAM_H
#define __CELL_STREAM_H

#include <string.h>

typedef void* cell_audio_handle_t;

typedef uint32_t (*cell_audio_sample_cb_t)(int16_t * out, uint32_t samples, void * userdata);

struct cell_audio_params
{
   uint32_t channels; // Audio channels.
   uint32_t samplerate; // Audio samplerate.
   uint32_t buffer_size; // Desired buffer size in bytes, if 0, a sane default will be provided.

   cell_audio_sample_cb_t sample_cb; // Can choose to use a callback for audio rather than blocking interface with write/write_avail. If this is not NULL, callback will be used. If NULL, you have to write audio using write/write_avail.

   void *userdata; // Custom userdata that is passed to sample_cb.

   const char *device; // Only used for RSound atm for IP address, etc.
   uint32_t headset;   // Set to 1 when headset has to be enabled
};

typedef struct cell_audio_driver
{
	cell_audio_handle_t (*init)(const struct cell_audio_params *params);

	int32_t (*write)(cell_audio_handle_t handle, const int16_t* data, uint32_t samples);
	uint32_t (*write_avail)(cell_audio_handle_t handle);

	void (*pause)(cell_audio_handle_t handle);
	int32_t (*unpause)(cell_audio_handle_t handle);
	uint32_t (*is_paused)(cell_audio_handle_t handle);
	uint32_t (*alive)(cell_audio_handle_t handle);

	void (*free)(cell_audio_handle_t handle);
} cell_audio_driver_t;


extern const cell_audio_driver_t cell_audio_audioport;
extern const cell_audio_driver_t cell_audio_rsound;

#endif
