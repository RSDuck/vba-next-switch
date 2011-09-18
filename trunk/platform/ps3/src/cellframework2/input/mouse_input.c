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
 * mouse_input.c - Cellframework Mk. II
 *
 *  Created on:   Feb 10, 2011
 *
 ********************************************************************************/

#include "mouse_input.h"

#define MAX_MICE 7

int cell_mouse_input_init(void)
{
   return cellMouseInit(MAX_MICE);
}

void cell_mouse_input_deinit(void)
{
   cellMouseEnd();
}

uint32_t cell_mouse_input_mice_connected(void)
{
   CellMouseInfo mouse_info;
   cellMouseGetInfo(&mouse_info);
   return mouse_info.now_connect;
}

CellMouseData cell_mouse_input_poll_device(uint32_t id)
{
   CellMouseData mouse_data;

   // Get new pad data
   cellMouseGetData(id, &mouse_data);

   return mouse_data;
}
