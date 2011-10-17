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

#define LAST_ASPECT_RATIO ASPECT_RATIO_CUSTOM

class PS3Graphics
{
	public:
		/* constructor/destructor */

		PS3Graphics(uint32_t resolution, uint32_t aspect, uint32_t smooth, uint32_t smooth2, const char * shader, const char * shader2, const char * menu_shader, uint32_t overscan, float overscan_amount, uint32_t pal60Hz, uint32_t vsync, uint32_t tripleBuffering, uint32_t viewport_x, uint32_t viewport_y, uint32_t viewport_width, uint32_t viewport_height); 
		~PS3Graphics();

		/* public variables */
		enum menu_type
		{
			TEXTURE_BACKDROP,
			TEXTURE_MENU
		};
		float aspectratios[LAST_ASPECT_RATIO];
		uint32_t aspect_x, aspect_y;
		uint32_t frame_count;

		/* PSGL functions */
		
		void Init(uint32_t scaleEnabled, uint32_t scaleFactor);
		void Deinit();

		/* draw functions */
		
		void Draw(uint8_t *XBuf);
		void DrawMenu(int width, int height);

		/* cg */
		
		int32_t InitCg();
		int32_t LoadFragmentShader(const char * shaderPath, unsigned index = 0);
		void ResetFrameCounter();
		void resize_aspect_mode_input_loop(uint64_t state);
		
		/* calculate functions */
		
		int calculate_aspect_ratio_before_game_load();

		/* get functions */
		
		int CheckResolution(uint32_t resId);
		int get_aspect_ratio_int(bool orientation);
		float get_aspect_ratio_float(int enum_);
		uint32_t GetCurrentResolution();
		uint32_t GetInitialResolution();
		uint32_t GetPAL60Hz();
		uint32_t get_viewport_x(void);
		uint32_t get_viewport_y(void);
		uint32_t get_viewport_width(void);
		uint32_t get_viewport_height(void);
		const char * GetFragmentShaderPath(unsigned index = 0) { return _curFragmentShaderPath[index]; }
		const char * GetResolutionLabel(uint32_t resolution);
		CellVideoOutState GetVideoOutState();
		
		int get_aspect_ratio_array(int enum_); //FIXME - is this needed?

		/* set functions */
		
		void set_aspect_ratio(uint32_t keep_aspect, uint32_t width = 0, uint32_t height = 0, uint32_t setviewport = 0);
		void SetFBOScale(uint32_t enable, unsigned scale = 2.0);
		void SetPAL60Hz(uint32_t pal60Hz);
		void SetOverscan(uint32_t overscan, float amount = 0.0f, uint32_t setviewport = 1);
		void SetSmooth(uint32_t smooth, unsigned index = 0);
		void SetTripleBuffering(uint32_t triple_buffering);
		void SetDimensions(unsigned width, unsigned height, unsigned pitch);
		void set_vsync(uint32_t vsync);
		uint32_t SetTextMessageSpeed(uint32_t value = 60);

		/* libdbgfont */
		void InitDbgFont();
		void dprintf_noswap(float x, float y, float scale, const char* fmt, ...);

		/* resolution functions */
		void ChangeResolution(uint32_t resId, uint16_t pal60Hz, uint16_t tripleBuffering, uint32_t scaleEnabled, uint32_t scaleFactor);
		void SwitchResolution(uint32_t resId, uint16_t pal60Hz, uint16_t tripleBuffering, uint32_t scaleEnabled, uint32_t scaleFactor);
		void NextResolution();
		void PreviousResolution();

		/* image png/jpg */
		bool LoadMenuTexture(enum menu_type type, const std::string &path);
	private:
		/* private variables */
		uint32_t fbo_enable;
		uint32_t m_tripleBuffering;
		uint32_t m_overscan;
		uint32_t m_pal60Hz;
		uint32_t m_smooth, m_smooth2;
		uint8_t *decode_buffer;
		uint32_t m_viewport_x, m_viewport_y, m_viewport_width, m_viewport_height, m_delta;
		uint32_t m_viewport_x_temp, m_viewport_y_temp, m_viewport_width_temp, m_viewport_height_temp, m_delta_temp;
		uint32_t m_vsync;
		int m_calculate_aspect_ratio_before_game_load;
		int m_currentResolutionPos;
		int m_resolutionId;
		uint32_t fbo_scale;
		uint32_t fbo_width, fbo_height;
		uint32_t fbo_vp_width, fbo_vp_height;
		uint32_t m_currentResolutionId;
		uint32_t m_initialResolution;
		float m_overscan_amount;
		float m_ratio;
		GLuint _cgViewWidth;
		GLuint _cgViewHeight;
		GLuint fbo_tex;
		GLuint fbo;
		GLuint gl_width;
		GLuint gl_height;
		GLuint tex;
		GLuint tex_menu;
		GLuint tex_backdrop;
		GLuint vbo[2];
		GLfloat m_left, m_right, m_bottom, m_top, m_zNear, m_zFar;
		char _curFragmentShaderPath[3][MAX_PATH_LENGTH];
		std::vector<uint32_t> m_supportedResolutions;
		CGcontext _cgContext;
		CGprogram _vertexProgram[3];
		CGprogram _fragmentProgram[3];
		CGparameter _cgpModelViewProj[3];
		CGparameter _cgpVideoSize[3];
		CGparameter _cgpTextureSize[3];
		CGparameter _cgpOutputSize[3];
		CGparameter _cgp_timer[3];
		CGparameter _cgp_vertex_timer[3];
		CGparameter _cgp_vertex_VideoSize[3];
		CGparameter _cgp_vertex_TextureSize[3];
		CGparameter _cgp_vertex_OutputSize[3];
		CellDbgFontConsoleId dbg_id;
		PSGLdevice* psgl_device;
		PSGLcontext* psgl_context;
		CellVideoOutState m_stored_video_state;
		/* emulator-specific private variables */
		unsigned m_width;
		unsigned m_height;
		unsigned m_pitch;

		/* image png/jpg */
		bool load_png(const std::string &path, unsigned &width, unsigned &height, uint8_t *data);
		bool load_jpeg(const std::string &path, unsigned &width, unsigned &height, uint8_t *data);
		void setup_texture(GLuint tex, unsigned width, unsigned height);

		/* PSGL */
		int32_t PSGLInit(uint32_t scaleEnable, uint32_t scaleFactor);
		void PSGLInitDevice(uint32_t resolutionId = 0, uint16_t pal60Hz = 0, uint16_t tripleBuffering = 1);
		void PSGLDeInitDevice();
		void init_fbo_memb();

		/* resolution functions */
		void SetResolution();

		/* get functions */
		void GetAllAvailableResolutions();

		/* set functions */
		void SetViewports();
};

extern char DEFAULT_SHADER_FILE[MAX_PATH_LENGTH];
extern char DEFAULT_MENU_SHADER_FILE[MAX_PATH_LENGTH];

#endif /* PS3Graphics_H_ */
