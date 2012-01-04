/******************************************************************************* 
 * ps3video.hpp - VBANext PS3
 *
 *  Created on: Nov 4, 2010
********************************************************************************/

#ifndef PS3GRAPHICS_H_
#define PS3GRAPHICS_H_

#include <string>
#include <cell/codec.h>

#include <vector>

#include <PSGL/psgl.h>
#include <PSGL/psglu.h>
#include <cell/dbgfont.h>

#include "colors.h"

#include "emu-ps3-constants.h"
#include "ps3input.h"

#ifdef CELL_DEBUG_PRINTF
	#define CELL_PRINTF(fmt, args...) do {\
	   gl_dprintf(0.1, 0.1, 1.0, fmt, ##args);\
	   sys_timer_usleep(CELL_DEBUG_PRINTF_DELAY);\
	   } while(0)
#else
#define CELL_PRINTF(fmt, args...) ((void)0)
#endif

/* emulator-specific */
#define SCREEN_RENDER_PIXEL_FORMAT GL_ARGB_SCE

/* resolution constants */
#define SCREEN_4_3_ASPECT_RATIO (4.0/3)
#define SCREEN_5_4_ASPECT_RATIO (5.0/4)
#define SCREEN_8_7_ASPECT_RATIO (8.0/7)
#define SCREEN_16_9_ASPECT_RATIO (16.0/9)
#define SCREEN_16_10_ASPECT_RATIO (16.0/10)
#define SCREEN_16_15_ASPECT_RATIO (16.0/15)
#define SCREEN_19_14_ASPECT_RATIO (19.0/14)
#define SCREEN_2_1_ASPECT_RATIO (2.0/1)
#define SCREEN_3_2_ASPECT_RATIO (3.0/2)
#define SCREEN_3_4_ASPECT_RATIO (3.0/4)
#define SCREEN_1_1_ASPECT_RATIO (1.0/1)
#define SCREEN_CUSTOM_ASPECT_RATIO (0.0/1)

enum
{
	ASPECT_RATIO_4_3,
	ASPECT_RATIO_5_4,
	ASPECT_RATIO_8_7,
	ASPECT_RATIO_16_9,
	ASPECT_RATIO_16_10,
	ASPECT_RATIO_16_15,
	ASPECT_RATIO_19_14,
	ASPECT_RATIO_2_1,
	ASPECT_RATIO_3_2,
	ASPECT_RATIO_3_4,
	ASPECT_RATIO_1_1,
	ASPECT_RATIO_AUTO,
	ASPECT_RATIO_CUSTOM
};

enum
{
	NORMAL,
	VERTICAL,
	FLIPPED,
	FLIPPED_ROTATED
};

#define MAX_ORIENTATION FLIPPED_ROTATED

enum menu_type
{
	TEXTURE_BACKDROP,
	TEXTURE_MENU
};

#define LAST_ASPECT_RATIO ASPECT_RATIO_CUSTOM

void ps3graphics_set_dimensions(unsigned width, unsigned height, unsigned pitch);

/* constructor/destructor */
void ps3graphics_new(uint32_t resolution, uint32_t aspect, uint32_t smooth, uint32_t smooth2, const char * shader, const char * shader2, const char * menu_shader, uint32_t overscan, float overscan_amount, uint32_t pal60Hz, uint32_t vsync, uint32_t tripleBuffering, uint32_t viewport_x, uint32_t viewport_y, uint32_t viewport_width, uint32_t viewport_height, uint32_t scale_enabled, uint32_t scale_factor); 

/* cg */

int32_t ps3graphics_load_fragment_shader(const char * shaderPath, unsigned index);
void ps3graphics_resize_aspect_mode_input_loop(uint64_t state);

/* draw functions */
void ps3graphics_draw(uint8_t * XBuf);
void ps3graphics_draw_menu(int width, int height);

/* image png/jpg */
bool ps3graphics_load_menu_texture(enum menu_type type, const char * path);

/* resolution functions */
void ps3graphics_change_resolution(uint32_t resId, uint16_t pal60Hz, uint16_t tripleBuffering, uint32_t scaleEnabled, uint32_t scaleFactor);
void ps3graphics_switch_resolution(uint32_t resId, uint16_t pal60Hz, uint16_t tripleBuffering, uint32_t scaleEnabled, uint32_t scaleFactor);
void ps3graphics_next_resolution();
void ps3graphics_previous_resolution();

/* calculate functions */

int ps3graphics_calculate_aspect_ratio_before_game_load();

void ps3graphics_update_state_uniforms(unsigned index);

/* get functions */
const char * ps3graphics_get_resolution_label(uint32_t resolution);
int ps3graphics_check_resolution(uint32_t resId);
int ps3graphics_get_aspect_ratio_int(bool orientation);
float ps3graphics_get_aspect_ratio_float(int enum_);
uint32_t ps3graphics_get_current_resolution();
uint32_t ps3graphics_get_initial_resolution();
uint32_t ps3graphics_get_pal60hz();
uint32_t ps3graphics_get_viewport_x(void);
uint32_t ps3graphics_get_viewport_y(void);
uint32_t ps3graphics_get_viewport_width(void);
uint32_t ps3graphics_get_viewport_height(void);
CellVideoOutState ps3graphics_get_video_out_state();
const char * ps3graphics_get_fragment_shader_path(unsigned index);

/* game-aware shaders */
void ps3graphics_init_state_uniforms(const char * path);

/* set functions */

void ps3graphics_set_aspect_ratio(uint32_t keep_aspect, uint32_t width, uint32_t height, uint32_t setviewport);
void ps3graphics_set_fbo_scale(uint32_t enable, unsigned scale);
void ps3graphics_set_pal60hz(uint32_t pal60Hz);
void ps3graphics_set_overscan(uint32_t will_overscan, float amount, uint32_t setviewport);
void ps3graphics_set_smooth(uint32_t smooth, unsigned index);
void ps3graphics_set_triple_buffering(uint32_t triple_buffering);
void ps3graphics_set_vsync(uint32_t vsync);
uint32_t ps3graphics_set_text_message_speed(uint32_t value);

/* orientation */
void ps3graphics_set_orientation(unsigned orientation);
unsigned ps3graphics_get_orientation_name (void);

/* libdbgfont */
void ps3graphics_init_dbgfont();

extern uint32_t frame_count;
//extern snes_tracker_t *tracker; // State tracker

/* emulator-specific private variables */
extern char DEFAULT_SHADER_FILE[MAX_PATH_LENGTH];
extern char DEFAULT_MENU_SHADER_FILE[MAX_PATH_LENGTH];


extern PSGLdevice* psgl_device;

extern "C" void _jsPlatformSwapBuffers( PSGLdevice* device);

#endif /* PS3Graphics_H_ */
