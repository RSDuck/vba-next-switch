/******************************************************************************* 
 * emu-ps3.hpp - VBANext PS3
 *
 *  Created on: May 4, 2011
********************************************************************************/

#ifndef EMUPS3_H_
#define EMUPS3_H_

#define AUDIO_BUFFER_SAMPLES (4096)

/* System includes */

#include <string>

#include <sdk_version.h>

#include <sysutil/sysutil_gamecontent.h>

#if(CELL_SDK_VERSION > 0x340000)
#include <sysutil/sysutil_screenshot.h>
#endif

/* Emulator-specific includes */

#include "../src/Util.h"
#include "../src/System.h"

/* PS3 frontend includes */

#include "cellframework2/input/pad_input.h"
#include "cellframework2/audio/stream.h"
#include "cellframework2/utility/oskutil.h"

#include "emu-ps3-constants.h"
#include "ps3input.h"
#include "ps3video.hpp"

#define input_code_state_begin() \
   uint64_t state = cell_pad_input_poll_device(0); \
   static uint64_t old_state = 0; \
   const uint64_t button_was_not_pressed = ~(state); \
   const uint64_t diff_state = old_state ^ state; \
   const uint64_t button_was_held = old_state & state; \
   const uint64_t button_was_not_held = ~(old_state & state); \
   const uint64_t button_was_pressed = old_state & diff_state;

/* PS3 frontend constants */

#define SOUND_MODE_NORMAL	1
#define SOUND_MODE_HEADSET	2
#define SOUND_MODE_RSOUND	3

#define FILETYPE_STATE		0
#define FILETYPE_BATTERY	1
#define FILETYPE_IMAGE_PREFS	2

#define WRITE_CONTROLS		0
#define READ_CONTROLS		1
#define SET_ALL_CONTROLS_TO_DEFAULT	2

enum {
   MENU_ITEM_LOAD_STATE = 0,
   MENU_ITEM_SAVE_STATE,
   MENU_ITEM_KEEP_ASPECT_RATIO,
   MENU_ITEM_OVERSCAN_AMOUNT,
   MENU_ITEM_ORIENTATION,
   MENU_ITEM_RESIZE_MODE,
   MENU_ITEM_FRAME_ADVANCE,
   MENU_ITEM_SCREENSHOT_MODE,
   MENU_ITEM_RESET,
   MENU_ITEM_RETURN_TO_GAME,
   MENU_ITEM_RETURN_TO_MENU,
#ifdef MULTIMAN_SUPPORT
   MENU_ITEM_RETURN_TO_MULTIMAN,
#endif
   MENU_ITEM_RETURN_TO_XMB
};

#define MENU_ITEM_LAST           MENU_ITEM_RETURN_TO_XMB

float Emulator_GetFontSize();
void Emulator_StopROMRunning();
void Emulator_StartROMRunning(uint32_t set_is_running = 1);
void Emulator_RequestLoadROM(const char * filename);
bool Emulator_IsROMLoaded();
void vba_toggle_sgb_border(bool set_border);
void emulator_save_settings(uint64_t filetosave);
extern void emulator_implementation_set_shader_preset(const char * fname);
void emulator_toggle_sound(uint64_t soundmode);
void emulator_set_controls(const char * config_file, int mapping_enum, const char * title);
void emulator_implementation_set_texture(const char * fname);
void set_text_message(const char * message, uint32_t speed);

extern int				srcWidth, srcHeight;
extern oskutil_params			oskutil_handle;

extern char contentInfoPath[MAX_PATH_LENGTH];
extern char usrDirPath[MAX_PATH_LENGTH];
extern char DEFAULT_PRESET_FILE[MAX_PATH_LENGTH];
extern char DEFAULT_BORDER_FILE[MAX_PATH_LENGTH];
extern char DEFAULT_MENU_BORDER_FILE[MAX_PATH_LENGTH];
extern char GAME_AWARE_SHADER_DIR_PATH[MAX_PATH_LENGTH];
extern char INPUT_PRESETS_DIR_PATH[MAX_PATH_LENGTH];
extern char PRESETS_DIR_PATH[MAX_PATH_LENGTH];
extern char BORDERS_DIR_PATH[MAX_PATH_LENGTH];
extern char SHADERS_DIR_PATH[MAX_PATH_LENGTH];
extern char DEFAULT_SHADER_FILE[MAX_PATH_LENGTH];
extern char DEFAULT_MENU_SHADER_FILE[MAX_PATH_LENGTH];
extern char SYS_CONFIG_FILE[MAX_PATH_LENGTH];

/* Emulator-specific extern variables */

extern uint32_t reinit_video;

#endif /* VBAPS3_H_ */
