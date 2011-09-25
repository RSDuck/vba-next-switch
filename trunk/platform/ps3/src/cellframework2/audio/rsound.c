#include "stream.h"
#include "rsound.h"
#include <stdlib.h>


typedef struct
{
   rsound_t *rd;
   volatile uint32_t is_alive;
   uint32_t is_paused;

   void *userdata;
   uint32_t (*audio_cb)(int16_t*, uint32_t, void*);
} rsd_t;

static void err_cb(void *userdata)
{
   rsd_t *rd = userdata;
   rd->is_alive = 0;
}

static ssize_t audio_cb(void *data, size_t bytes, void *userdata)
{
   rsd_t *rd = userdata;
   ssize_t ret = rd->audio_cb(data, bytes / sizeof(int16_t), rd->userdata);
   return ret * sizeof(int16_t);
}

static cell_audio_handle_t rsound_init(const struct cell_audio_params *params)
{
   rsd_t *rd = calloc(1, sizeof(*rd));
   if (!rd)
      return NULL;

   if (rsd_init(&rd->rd) < 0)
      goto error;

   rd->is_alive = 1;
   int chans = params->channels;
   int rate = params->samplerate;

   int latency = params->buffer_size * 1000 / (rate * chans * sizeof(int16_t));
   if (latency == 0)
      latency = 64;

   rsd_set_param(rd->rd, RSD_CHANNELS, &chans);
   rsd_set_param(rd->rd, RSD_SAMPLERATE, &rate);
   rsd_set_param(rd->rd, RSD_LATENCY, &latency);

   int format = RSD_S16_NE;
   rsd_set_param(rd->rd, RSD_FORMAT, &format);

   if (params->device)
      rsd_set_param(rd->rd, RSD_HOST, (void*)params->device);

   if (params->sample_cb)
   {
      rd->userdata = params->userdata;
      rd->audio_cb = params->sample_cb;
      rsd_set_callback(rd->rd, audio_cb, err_cb, 1024, rd); 
   }

   if (rsd_start(rd->rd) < 0)
      goto error;

   return rd;

error:
   if (rd->rd)
      rsd_free(rd->rd);
   free(rd);
   return NULL;
}

static int32_t rsound_write(cell_audio_handle_t handle, const int16_t* data, uint32_t samples)
{
   rsd_t *rd = handle;
   rsd_delay_wait(rd->rd);
   if (rsd_write(rd->rd, data, samples * sizeof(int16_t)) == 0)
      return -1;
   return samples;
}

static uint32_t rsound_write_avail(cell_audio_handle_t handle)
{
   rsd_t *rd = handle;
   return rsd_get_avail(rd->rd) / sizeof(int16_t);
}

static void rsound_pause(cell_audio_handle_t handle)
{
   rsd_t *rd = handle;
   rd->is_paused = 1;
   rsd_pause(rd->rd, 1);
}

static int32_t rsound_unpause(cell_audio_handle_t handle)
{
   rsd_t *rd = handle;
   rd->is_paused = 0;
   int ret = rsd_pause(rd->rd, 0);

   if (ret < 0)
      rd->is_alive = 0;
   else
      rd->is_alive = 1;

   return (int32_t)ret;
}

static uint32_t rsound_is_paused(cell_audio_handle_t handle)
{
   rsd_t *rd = handle;
   return rd->is_paused;
}

static void rsound_free(cell_audio_handle_t handle)
{
   rsd_t *rd = handle;
   rsd_stop(rd->rd);
   rsd_free(rd->rd);
   free(rd);
}

static uint32_t rsound_alive(cell_audio_handle_t handle)
{
   rsd_t *rd = handle;
   return rd->is_alive;
}

const cell_audio_driver_t cell_audio_rsound = {
   .init = rsound_init,
   .write = rsound_write,
   .write_avail = rsound_write_avail,
   .pause = rsound_pause,
   .unpause = rsound_unpause,
   .is_paused = rsound_is_paused,
   .alive = rsound_alive,
   .free = rsound_free
};
