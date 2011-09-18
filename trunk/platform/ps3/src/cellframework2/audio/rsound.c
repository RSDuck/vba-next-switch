#include "stream.h"
#include "rsound.h"
#include <stdlib.h>


typedef struct
{
   rsound_t *rd;
   volatile u32 is_alive;
   u32 is_paused;

   void *userdata;
   u32 (*audio_cb)(s16*, u32, void*);
} rsd_t;

static void err_cb(void *userdata)
{
   rsd_t *rd = userdata;
   rd->is_alive = 0;
}

static ssize_t audio_cb(void *data, size_t bytes, void *userdata)
{
   rsd_t *rd = userdata;
   ssize_t ret = rd->audio_cb(data, bytes / sizeof(s16), rd->userdata);
   return ret * sizeof(s16);
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

   int latency = params->buffer_size * 1000 / (rate * chans * sizeof(s16));
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

static s32 rsound_write(cell_audio_handle_t handle, const s16* data, u32 samples)
{
   rsd_t *rd = handle;
   rsd_delay_wait(rd->rd);
   if (rsd_write(rd->rd, data, samples * sizeof(s16)) == 0)
      return -1;
   return samples;
}

static u32 rsound_write_avail(cell_audio_handle_t handle)
{
   rsd_t *rd = handle;
   return rsd_get_avail(rd->rd) / sizeof(s16);
}

static void rsound_pause(cell_audio_handle_t handle)
{
   rsd_t *rd = handle;
   rd->is_paused = 1;
   rsd_pause(rd->rd, 1);
}

static s32 rsound_unpause(cell_audio_handle_t handle)
{
   rsd_t *rd = handle;
   rd->is_paused = 0;
   int ret = rsd_pause(rd->rd, 0);

   if (ret < 0)
      rd->is_alive = 0;
   else
      rd->is_alive = 1;

   return (s32)ret;
}

static u32 rsound_is_paused(cell_audio_handle_t handle)
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

static u32 rsound_alive(cell_audio_handle_t handle)
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
