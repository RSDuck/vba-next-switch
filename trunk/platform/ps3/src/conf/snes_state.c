/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "snes_state.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../cellframework2/input/pad_input.h"

struct snes_tracker_internal
{
   char id[64];

   // The pointer we point to can apparently change per-game.
   const uint8_t **ptr;
   const uint16_t *input_ptr;
   bool is_input;

   uint32_t addr;
   unsigned mask;

   enum snes_tracker_type type;

   uint32_t prev[2];
   int frame_count;
   int frame_count_prev;
   uint32_t old_value; 
   int transition_count;
};

struct snes_tracker
{
   struct snes_tracker_internal *info;
   unsigned info_elem;

   uint16_t input_state[2];
};

snes_tracker_t* snes_tracker_init(const struct snes_tracker_info *info)
{
   snes_tracker_t *tracker = calloc(1, sizeof(*tracker));
   if (!tracker)
      return NULL;

   tracker->info = calloc(info->info_elem, sizeof(struct snes_tracker_internal));
   tracker->info_elem = info->info_elem;

   for (unsigned i = 0; i < info->info_elem; i++)
   {
      strncpy(tracker->info[i].id, info->info[i].id, sizeof(tracker->info[i].id));
      tracker->info[i].addr = info->info[i].addr;
      tracker->info[i].type = info->info[i].type;
      tracker->info[i].mask = (info->info[i].mask == 0) ? 0xffffffffu : info->info[i].mask;

      switch (info->info[i].ram_type)
      {
         case SSNES_STATE_WRAM:
            tracker->info[i].ptr = info->wram;
            break;
         case SSNES_STATE_APURAM:
            tracker->info[i].ptr = info->apuram;
            break;
         case SSNES_STATE_OAM:
            tracker->info[i].ptr = info->oam;
            break;
         case SSNES_STATE_CGRAM:
            tracker->info[i].ptr = info->cgram;
            break;
         case SSNES_STATE_VRAM:
            tracker->info[i].ptr = info->vram;
            break;

         case SSNES_STATE_INPUT_SLOT1:
            tracker->info[i].input_ptr = &tracker->input_state[0];
            tracker->info[i].is_input = true;
            break;
         case SSNES_STATE_INPUT_SLOT2:
            tracker->info[i].input_ptr = &tracker->input_state[1];
            tracker->info[i].is_input = true;
            break;

         default:
            tracker->info[i].ptr = NULL;
      }
   }


   return tracker;
}

void snes_tracker_free(snes_tracker_t *tracker)
{
   free(tracker->info);
   free(tracker);
}

#define fetch() ((info->is_input ? *info->input_ptr : ((*(info->ptr))[info->addr])) & info->mask)

static void update_element(
      struct snes_tracker_uniform *uniform,
      struct snes_tracker_internal *info,
      unsigned frame_count)
{
   uniform->id = info->id;

   switch (info->type)
   {
      case SSNES_STATE_CAPTURE:
         uniform->value = fetch();
         break;

      case SSNES_STATE_CAPTURE_PREV:
         if (info->prev[0] != fetch())
         {
            info->prev[1] = info->prev[0];
            info->prev[0] = fetch();
         }
         uniform->value = info->prev[1];
         break;

      case SSNES_STATE_TRANSITION:
         if (info->old_value != fetch())
         {
            info->old_value = fetch();;
            info->frame_count = frame_count;
         }
         uniform->value = info->frame_count;
         break;

      case SSNES_STATE_TRANSITION_PREV:
         if (info->old_value != fetch())
         {
            info->old_value = fetch();
            info->frame_count_prev = info->frame_count;
            info->frame_count = frame_count;
         }
         uniform->value = info->frame_count_prev;
         break;

      case SSNES_STATE_TRANSITION_COUNT:
         if (info->old_value != fetch())
         {
            info->old_value = fetch();
            info->transition_count++;
         }
         uniform->value = info->transition_count;
         break;
      
      default:
         break;
   }
}

#undef fetch

static void update_input(snes_tracker_t *tracker)
{
   cell_input_state_t input[2];
   input[0] = cell_pad_input_poll_device(0);
   input[1] = cell_pad_input_poll_device(1);

   uint16_t state[2] = {0};
   static const uint16_t masks[] = {
      CTRL_R1_MASK,
      CTRL_L1_MASK,
      CTRL_TRIANGLE_MASK,
      CTRL_CIRCLE_MASK,
      CTRL_RIGHT_MASK,
      CTRL_LEFT_MASK,
      CTRL_DOWN_MASK,
      CTRL_UP_MASK,
      CTRL_START_MASK,
      CTRL_SELECT_MASK,
      CTRL_SQUARE_MASK,
      CTRL_CROSS_MASK
   };

   for (unsigned i = 4; i < 16; i++)
   {
      for (unsigned j = 0; j < 1; j++)
         state[j] |= (CTRL_MASK(masks[i - 4], input[j]) ? 1 : 0) << i;
   }

   for (unsigned i = 0; i < 2; i++)
      tracker->input_state[i] = state[i];
}

unsigned snes_get_uniform(snes_tracker_t *tracker, struct snes_tracker_uniform *uniforms, unsigned elem, unsigned frame_count)
{
   update_input(tracker);

   unsigned elems = tracker->info_elem < elem ? tracker->info_elem : elem;

   for (unsigned i = 0; i < elems; i++)
      update_element(&uniforms[i], &tracker->info[i], frame_count);

   return elems;
}

