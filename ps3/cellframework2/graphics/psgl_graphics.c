/******************************************************************************* 
 *  -- Cellframework Mk.II -  Open framework to abstract the common tasks related to
 *                            PS3 application development.
 *
 *  Copyright (C) 2010-2011
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 ********************************************************************************/

/******************************************************************************* 
 * PSGLGraphics.c - Cellframework
 *
 *  Created on:		Jan 30, 2011
 *
 * CONVENTIONS AS OF NOW:
 * Return values follow POSIX convention. 0 for success, and negative numbers for failure.
 ********************************************************************************/

#include "psgl_graphics.h"
#include <psgl/psgl.h>
#include <stdlib.h>
#include <cell/dbgfont.h>

struct cell_psgl_device
{
   PSGLcontext* psgl_context;
   PSGLdevice* psgl_device;
   GLuint psgl_height;
   GLuint psgl_width;

   u32 triple_buffering_enabled;
   u32 vsync_enabled;
   u32 pal_60hz_enabled;

   CellVideoOutState video_state;
};

static int cell_psgl_setup(cell_psgl_device_t *device, const struct cell_psgl_resolution_params *client_params)
{
   PSGLdeviceParameters params;
   PSGLinitOptions options = {
      .enable = PSGL_INIT_MAX_SPUS | PSGL_INIT_INITIALIZE_SPUS | PSGL_INIT_HOST_MEMORY_SIZE,
      .maxSPUs = 1,
      .initializeSPUs = GL_TRUE,
      .hostMemorySize = client_params && client_params->host_memory ? client_params->host_memory : 8 * (1 << 20),
   };

   psglInit(&options);

   params.enable = PSGL_DEVICE_PARAMETERS_COLOR_FORMAT |
      PSGL_DEVICE_PARAMETERS_DEPTH_FORMAT |
      PSGL_DEVICE_PARAMETERS_MULTISAMPLING_MODE;

   params.colorFormat = GL_ARGB_SCE;
   params.depthFormat = GL_NONE;
   params.multisamplingMode = GL_MULTISAMPLING_NONE_SCE;

   if (client_params && client_params->triple_buffering)
   {
      params.enable |= PSGL_DEVICE_PARAMETERS_BUFFERING_MODE;
      params.bufferingMode = PSGL_BUFFERING_MODE_TRIPLE;
      device->triple_buffering_enabled = 1;
   }

   if (client_params && client_params->pal_60hz)
   {
      params.enable |= PSGL_DEVICE_PARAMETERS_RESC_PAL_TEMPORAL_MODE;
      params.rescPalTemporalMode = RESC_PAL_TEMPORAL_MODE_60_INTERPOLATE;
      params.enable |= PSGL_DEVICE_PARAMETERS_RESC_RATIO_MODE;
      params.rescRatioMode = RESC_RATIO_MODE_FULLSCREEN;
      device->pal_60hz_enabled = 1;
      device->vsync_enabled = 1; // Using libresc forces VSync.
   }

   if (client_params && client_params->resolution_id)
   {
      //Resolution setting
      CellVideoOutResolution resolution;
      cellVideoOutGetResolution(client_params->resolution_id, &resolution);

      params.enable |= PSGL_DEVICE_PARAMETERS_WIDTH_HEIGHT;
      params.width = resolution.width;
      params.height = resolution.height;
   }

   device->psgl_device = psglCreateDeviceExtended(&params);
   if(!device->psgl_device)
      return -1;

   // Get the dimensions of the screen in question, and do stuff with it :)
   psglGetDeviceDimensions(device->psgl_device, &device->psgl_width, &device->psgl_height); 

   // Create a context and bind it to the current display.
   device->psgl_context = psglCreateContext();

   psglMakeCurrent(device->psgl_context, device->psgl_device);

   psglResetCurrentContext();
   return 0;
}

cell_psgl_device_t *cell_psgl_init(const struct cell_psgl_resolution_params *params)
{
   cell_psgl_device_t *device = calloc(1, sizeof(*device));
   if (!device)
      return NULL;

   if (cell_psgl_setup(device, params) < 0)
   {
      free(device);
      return NULL;
   }

   return device;
}

static inline int cell_psgl_get_video_state(cell_psgl_device_t *device)
{
   return cellVideoOutGetState(CELL_VIDEO_OUT_PRIMARY, 0, &device->video_state);
}

static void cell_psgl_reset(cell_psgl_device_t *device)
{
   cell_psgl_deinit_debug_font(device);
   psglDestroyContext(device->psgl_context);
   psglDestroyDevice(device->psgl_device);
   device->psgl_width = 0;
   device->psgl_height = 0;
   device->triple_buffering_enabled = 0;
   device->pal_60hz_enabled = 0;
   device->psgl_context = NULL;
   device->psgl_device = NULL;
}

void cell_psgl_free(cell_psgl_device_t *device)
{
   glFinish();
   cell_psgl_reset(device);
#if CELL_SDK_VERSION == 0x340001
   psglExit();
#endif
   free(device);
}

u32 cell_psgl_query_resolution_avail(cell_psgl_device_t *device, u32 resolution_id)
{
   (void)device;
   return cellVideoOutGetResolutionAvailability(CELL_VIDEO_OUT_PRIMARY, resolution_id, CELL_VIDEO_OUT_ASPECT_AUTO, 0);
}

u32 cell_psgl_query_all_resolutions(cell_psgl_device_t *device, u32 *resolutions, u32 num_elems)
{
   const u32 video_modes[] = {
      CELL_VIDEO_OUT_RESOLUTION_480,
      CELL_VIDEO_OUT_RESOLUTION_576,
      CELL_VIDEO_OUT_RESOLUTION_960x1080,
      CELL_VIDEO_OUT_RESOLUTION_720,
      CELL_VIDEO_OUT_RESOLUTION_1280x1080,
      CELL_VIDEO_OUT_RESOLUTION_1440x1080,
      CELL_VIDEO_OUT_RESOLUTION_1600x1080,
      CELL_VIDEO_OUT_RESOLUTION_1080
   };

   u32 ret = 0;
   for (unsigned i = 0; i < sizeof(video_modes)/sizeof(video_modes[0]) && ret < num_elems; i++)
   {
      if (cell_psgl_query_resolution_avail(device, video_modes[i]))
      {
         resolutions[ret++] = video_modes[i];
      }
   }

   return ret;
}

static int cell_psgl_change_resolution(cell_psgl_device_t *device, const struct cell_psgl_resolution_params *params)
{
   cell_psgl_reset(device);
   if (cell_psgl_setup(device, params) < 0)
      return -1;

   if (cell_psgl_init_debug_font(device) < 0)
      return -1;
   if (cell_psgl_get_video_state(device) < 0)
      return -1;
   return 0;
}

int cell_psgl_switch_resolution(cell_psgl_device_t *device, const struct cell_psgl_resolution_params *params)
{
   if(cell_psgl_query_resolution_avail(device, params->resolution_id))
   {
      if (cell_psgl_change_resolution(device, params) < 0)
         return -1;
      return 0;
   }
   return -1;
}

void cell_psgl_deinit_debug_font(cell_psgl_device_t *device)
{
   (void)device;
   cellDbgFontExit();
}

int cell_psgl_init_debug_font(cell_psgl_device_t *device)
{
   CellDbgFontConfig cfg = {
      .bufSize = 512,
      .screenWidth = device->psgl_width,
      .screenHeight = device->psgl_height,
   };
   return cellDbgFontInit(&cfg);
}

float cell_psgl_get_aspect_ratio(cell_psgl_device_t *device)
{
   return psglGetDeviceAspectRatio(device->psgl_device);
}

void cell_psgl_get_resolution(cell_psgl_device_t *device, u32 *width, u32 *height)
{
   *width = device->psgl_width;
   *height = device->psgl_height;
}

u32 cell_psgl_get_resolution_id(cell_psgl_device_t *device)
{
   cell_psgl_get_video_state(device);
   return device->video_state.displayMode.resolutionId;
}

