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
 * mouse_input.h - Cellframework Mk. II
 *
 *  Created on:   Feb 10, 2011
 ********************************************************************************/

#ifndef __CELL_MOUSE_INPUT_H
#define __CELL_MOUSE_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cell/mouse.h>

// Init and destruction of device.
int cell_mouse_input_init(void);
void cell_mouse_input_deinit(void);

// Get number of pads connected
uint32_t cell_mouse_input_mice_connected(void);

CellMouseData cell_mouse_input_poll_device(uint32_t id);

#ifdef __cplusplus
}
#endif

#endif
