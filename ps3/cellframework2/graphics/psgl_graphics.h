/*************************************************************************************
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
 * PSGLGraphics.h - Cellframework
 *
 *  Created on:		Jan 30, 2011
 ********************************************************************************/

#ifndef __CELL_PSGL_GRAPHICS_H
#define __CELL_PSGL_GRAPHICS_H

#include "../common/celltypes.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cell_psgl_resolution_params
{
   u32 resolution_id;
   u32 pal_60hz;
   u32 triple_buffering;
   u32 host_memory;
};

typedef struct cell_psgl_device cell_psgl_device_t;

// Init and destruction of device. If passing in NULL for params, some defaults are assumed.
// Returns NULL if failed.
cell_psgl_device_t *cell_psgl_init(const struct cell_psgl_resolution_params *params);
void cell_psgl_free(cell_psgl_device_t *device);

// Query resolution and aspect ratios. Aspect ratio may differ from width / height!
float cell_psgl_get_aspect_ratio(cell_psgl_device_t *device);
void cell_psgl_get_resolution(cell_psgl_device_t *device, u32 *width, u32 *height);
u32 cell_psgl_get_resolution_id(cell_psgl_device_t *device);

// Init and deinit debug fonts.
int cell_psgl_init_debug_font(cell_psgl_device_t *device);
void cell_psgl_deinit_debug_font(cell_psgl_device_t *device);

// Change resolution of the device.
int cell_psgl_switch_resolution(cell_psgl_device_t *device, const struct cell_psgl_resolution_params *params);

// Query for availability for a certain resolution. Returns non-zero if available.
u32 cell_psgl_query_resolution_avail(cell_psgl_device_t *device, u32 resolution_id);

// Fills in an array with all available resolutions. Returns number of resolutions available.
// num_elems states the number of elements available to write. If return value equals num_elems,
// considering calling this function again to get all available resolutions.
u32 cell_psgl_query_all_resolutions(cell_psgl_device_t *device, u32 *resolutions, u32 num_elems);

#ifdef __cplusplus
}
#endif

#endif
