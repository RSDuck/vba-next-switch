/******************************************************************************* 
 * ps3video.cpp - VBANext PS3
 *
 *  Created on: Nov 14, 2010
********************************************************************************/

#ifdef CELL_DEBUG_FPS
#include <sys/sys_time.h>
#endif

#include "cellframework2/input/pad_input.h"

#include "ps3video.hpp"

#define MENU_SHADER_NO 2

/* public  variables */
static float aspectratios[LAST_ASPECT_RATIO];
static uint32_t aspect_x, aspect_y;
uint32_t frame_count;

/* private variables */
static uint32_t fbo_enable;
static uint32_t m_tripleBuffering;
static uint32_t m_overscan;
static uint32_t m_pal60Hz;
static uint32_t m_smooth, m_smooth2;
static uint8_t *decode_buffer;
static uint32_t m_viewport_x, m_viewport_y, m_viewport_width, m_viewport_height;
static uint32_t m_viewport_x_temp, m_viewport_y_temp, m_viewport_width_temp, m_viewport_height_temp;
static uint32_t m_vsync;
static int m_calculate_aspect_ratio_before_game_load;
static int m_currentResolutionPos;
static int m_resolutionId;
static uint32_t fbo_scale;
static uint32_t fbo_width, fbo_height;
static uint32_t fbo_vp_width, fbo_vp_height;
static uint32_t m_currentResolutionId;
static uint32_t m_initialResolution;
static float m_overscan_amount;
static float m_ratio;
static GLuint _cgViewWidth;
static GLuint _cgViewHeight;
static GLuint fbo_tex;
static GLuint fbo;
static GLuint gl_width;
static GLuint gl_height;
static GLuint tex;
static GLuint tex_menu;
static GLuint tex_backdrop;
static GLuint vbo[2];
static unsigned m_width, m_height, m_pitch;
static unsigned m_orientation;

static GLfloat m_left, m_right, m_bottom, m_top, m_zNear, m_zFar;
static char curFragmentShaderPath[3][MAX_PATH_LENGTH];

static std::vector<uint32_t> m_supportedResolutions;
static CGcontext _cgContext;
static CGprogram _vertexProgram[3];
static CGprogram _fragmentProgram[3];

/* cg uniform variables */
static CGparameter _cgpModelViewProj[3];
static CGparameter _cgpVideoSize[3];
static CGparameter _cgpTextureSize[3];
static CGparameter _cgpOutputSize[3];
static CGparameter _cgp_vertex_VideoSize[3];
static CGparameter _cgp_vertex_TextureSize[3];
static CGparameter _cgp_vertex_OutputSize[3];
static CGparameter _cgp_timer[3];
static CGparameter _cgp_vertex_timer[3];

PSGLdevice* psgl_device;
static PSGLcontext* psgl_context;
static CellVideoOutState m_stored_video_state;
/* snes_tracker_t *tracker; */
/* static std::vector<lookup_texture> lut_textures; */

static GLfloat texcoords[] = {
	0, 1,		/* top-left*/
	0, 0,		/* bottom-left*/
	1, 0,		/* bottom-right*/
	1, 1		/* top-right*/
};

static GLfloat texcoords_vertical[] = {
	1, 1,		/* top-right*/
	0, 1,		/* top-left*/
	0, 0,		/* bottom-left*/
	1, 0		/* bottom-right*/
};

static GLfloat texcoords_flipped[] = {
	1, 0,		/* bottom-right*/
	1, 1,		/* top-right*/
	0, 1,		/* top-left*/
	0, 0		/* bottom-left*/
};

static GLfloat texcoords_flipped_rotated[] = {
	0, 0,		/* bottom-left*/
	1, 0,		/* bottom-right*/
	1, 1,		/* top-right*/
	0, 1		/* top-left*/
};

/******************************************************************************* 
	Calculate macros
********************************************************************************/

static void CalculateViewports (void)
{
	float delta, device_aspect;
	GLuint temp_width, temp_height;

	device_aspect = psglGetDeviceAspectRatio(psgl_device);
	temp_width = gl_width;
	temp_height = gl_height;

	/* calculate the glOrtho matrix needed to transform the texture to the 
	   desired aspect ratio.

	   If the aspect ratios of screen and desired aspect ratio are 
	   sufficiently equal (floating point stuff), assume they are actually equal */

	if (m_ratio == SCREEN_CUSTOM_ASPECT_RATIO)
	{
		m_viewport_x_temp = m_viewport_x;
		m_viewport_y_temp = m_viewport_y;
		m_viewport_width_temp = m_viewport_width;
		m_viewport_height_temp = m_viewport_height;
	}
	else if ( (int)(device_aspect*1000) > (int)(m_ratio * 1000) )
	{
		delta = (m_ratio / device_aspect - 1.0) / 2.0 + 0.5;
		m_viewport_x_temp = temp_width * (0.5f - delta);
		m_viewport_y_temp = 0;
		m_viewport_width_temp = (int)(2.0 * temp_width * delta);
		m_viewport_height_temp = temp_height;
	}
	else if ( (int)(device_aspect*1000) < (int)(m_ratio * 1000) )
	{
		delta = (device_aspect / m_ratio - 1.0) / 2.0 + 0.5;
		m_viewport_x_temp = 0;
		m_viewport_y_temp = temp_height * (0.5f - delta);
		m_viewport_width_temp = temp_width;
		m_viewport_height_temp = (int)(2.0 * temp_height * delta);
	}
	else
	{
		m_viewport_x_temp = 0;
		m_viewport_y_temp = 0;
		m_viewport_width_temp = temp_width;
		m_viewport_height_temp = temp_height;
	}
	if (m_overscan)
	{
		m_left = -m_overscan_amount/2;
		m_right = 1 + m_overscan_amount/2;
		m_bottom = -m_overscan_amount/2;
		m_top = 1 + m_overscan_amount/2;
		m_zFar = -1;
		m_zNear = 1;
	}
	else
	{
		m_left = 0;
		m_right = 1;
		m_bottom = 0;
		m_top = 1;
		m_zNear = -1;
		m_zFar = 1;
	}
	_cgViewWidth = m_viewport_width_temp;
	_cgViewHeight = m_viewport_height_temp;
}

#define setup_aspect_ratio_array() \
   aspectratios[ASPECT_RATIO_4_3] = SCREEN_4_3_ASPECT_RATIO; \
   aspectratios[ASPECT_RATIO_5_4] = SCREEN_5_4_ASPECT_RATIO; \
   aspectratios[ASPECT_RATIO_8_7] = SCREEN_8_7_ASPECT_RATIO; \
   aspectratios[ASPECT_RATIO_16_9] = SCREEN_16_9_ASPECT_RATIO; \
   aspectratios[ASPECT_RATIO_16_10] = SCREEN_16_10_ASPECT_RATIO; \
   aspectratios[ASPECT_RATIO_16_15] = SCREEN_16_15_ASPECT_RATIO; \
   aspectratios[ASPECT_RATIO_19_14] = SCREEN_19_14_ASPECT_RATIO; \
   aspectratios[ASPECT_RATIO_2_1] = SCREEN_2_1_ASPECT_RATIO; \
   aspectratios[ASPECT_RATIO_3_2] = SCREEN_3_2_ASPECT_RATIO; \
   aspectratios[ASPECT_RATIO_3_4] = SCREEN_3_4_ASPECT_RATIO; \
   aspectratios[ASPECT_RATIO_1_1] = SCREEN_1_1_ASPECT_RATIO; \
   aspectratios[ASPECT_RATIO_AUTO] = 0.0/0.0;

/*******************************************************************************
	Set macros
********************************************************************************/

#define SetViewports() \
	glMatrixMode(GL_PROJECTION); \
	glLoadIdentity(); \
	glViewport(m_viewport_x_temp, m_viewport_y_temp, m_viewport_width_temp, m_viewport_height_temp); \
	glOrthof(m_left, m_right, m_bottom, m_top, m_zNear, m_zFar); \
	glMatrixMode(GL_MODELVIEW); \
	glLoadIdentity();

#define SetScreenDimensions(width, height, pitch, set_dimensions) \
{ \
	if(set_dimensions) \
	{ \
	   m_width = width; \
	   m_height = height; \
	} \
	m_pitch = pitch; \
	glBufferData(GL_TEXTURE_REFERENCE_BUFFER_SCE, m_height * m_pitch, NULL, GL_STREAM_DRAW); \
}

/******************************************************************************* 
	Calculate functions
********************************************************************************/

int ps3graphics_calculate_aspect_ratio_before_game_load()
{
   return m_calculate_aspect_ratio_before_game_load;
}

/******************************************************************************* 
	PSGL
********************************************************************************/
static void init_fbo_memb()
{
	fbo = 0;
	fbo_tex = 0;
	tex_menu = 0;
	tex_backdrop = 0;

	fbo_enable = false;

	for(int i = 0; i < 3; i++)
	{
		_vertexProgram[i] = NULL;
		_fragmentProgram[i] = NULL;
	}

	/*tracker = NULL;*/
}

static void ps3graphics_psgl_init_device(uint32_t resolutionId, uint16_t pal60Hz, uint16_t tripleBuffering)
{
	PSGLdeviceParameters params;
	PSGLinitOptions options;
	options.enable = PSGL_INIT_MAX_SPUS | PSGL_INIT_INITIALIZE_SPUS;
#if CELL_SDK_VERSION == 0x340001
	options.enable |= PSGL_INIT_TRANSIENT_MEMORY_SIZE;
#else
	options.enable |=	PSGL_INIT_HOST_MEMORY_SIZE;
#endif
	options.maxSPUs = 1;
	options.initializeSPUs = GL_FALSE;
	options.persistentMemorySize = 0;
	options.transientMemorySize = 0;
	options.errorConsole = 0;
	options.fifoSize = 0;
	options.hostMemorySize = 0;

	psglInit(&options);

	params.enable = PSGL_DEVICE_PARAMETERS_COLOR_FORMAT | \
			PSGL_DEVICE_PARAMETERS_DEPTH_FORMAT | \
			PSGL_DEVICE_PARAMETERS_MULTISAMPLING_MODE;
	params.colorFormat = GL_ARGB_SCE;
	params.depthFormat = GL_NONE;
	params.multisamplingMode = GL_MULTISAMPLING_NONE_SCE;

	if (tripleBuffering)
	{
		params.enable |= PSGL_DEVICE_PARAMETERS_BUFFERING_MODE;
		params.bufferingMode = PSGL_BUFFERING_MODE_TRIPLE;
	}

	if (pal60Hz) {
		params.enable |= PSGL_DEVICE_PARAMETERS_RESC_PAL_TEMPORAL_MODE;
		params.rescPalTemporalMode = RESC_PAL_TEMPORAL_MODE_60_INTERPOLATE;
		params.enable |= PSGL_DEVICE_PARAMETERS_RESC_RATIO_MODE;
		params.rescRatioMode = RESC_RATIO_MODE_FULLSCREEN;
	}

	if (resolutionId) {
		CellVideoOutResolution resolution;
		cellVideoOutGetResolution(resolutionId, &resolution);

		params.enable |= PSGL_DEVICE_PARAMETERS_WIDTH_HEIGHT;
		params.width = resolution.width;
		params.height = resolution.height;
		m_currentResolutionId = resolutionId;
	}

	psgl_device = psglCreateDeviceExtended(&params);

	/* Get the dimensions of the screen in question, and do stuff with it :)*/
	psglGetDeviceDimensions(psgl_device, &gl_width, &gl_height);

	if(m_viewport_width == 0)
		m_viewport_width = gl_width;
	if(m_viewport_height == 0)
		m_viewport_height = gl_height;

	/* Create a context and bind it to the current display.*/
	psgl_context = psglCreateContext();

	psglMakeCurrent(psgl_context, psgl_device);

	psglResetCurrentContext();
}

int32_t ps3graphics_init_cg()
{
	cgRTCgcInit();

	_cgContext = cgCreateContext();
	if (_cgContext == NULL)
	{
		printf("Error creating Cg Context\n");
		return 1;
	}

	for (unsigned i = 0; i < 3; i++)
	{
		if (strlen(curFragmentShaderPath[i]) > 0)
		{
			if( ps3graphics_load_fragment_shader(curFragmentShaderPath[i], i) != CELL_OK)
				return 1;
		}
		else
		{
			strcpy(curFragmentShaderPath[i], DEFAULT_SHADER_FILE);
			if (ps3graphics_load_fragment_shader(curFragmentShaderPath[i], i) != CELL_OK)
				return 1;
		}
	}

	return CELL_OK;
}

unsigned ps3graphics_get_orientation_name (void)
{
	return m_orientation;
}

void ps3graphics_set_orientation(unsigned orientation)
{
	GLfloat vertex_buf[128];
	GLfloat vertexes[] = {
		0, 0,
		0, 0,
		1, 0,
		1, 1,
		0, 1,
		0, 0,
	};

	memcpy(vertex_buf, vertexes, 12 * sizeof(GLfloat));
	m_orientation = orientation;

	switch(orientation)
	{
		case NORMAL:
			memcpy(vertex_buf + 32, texcoords, 8 * sizeof(GLfloat));
			memcpy(vertex_buf + 32 * 3, texcoords, 8 * sizeof(GLfloat));
			break;
		case VERTICAL:
			memcpy(vertex_buf + 32, texcoords_vertical, 8 * sizeof(GLfloat));
			memcpy(vertex_buf + 32 * 3, texcoords_vertical, 8 * sizeof(GLfloat));
			break;
		case FLIPPED:
			memcpy(vertex_buf + 32, texcoords_flipped, 8 * sizeof(GLfloat));
			memcpy(vertex_buf + 32 * 3, texcoords_flipped, 8 * sizeof(GLfloat));
			break;
		case FLIPPED_ROTATED:
			memcpy(vertex_buf + 32, texcoords_flipped_rotated, 8 * sizeof(GLfloat));
			memcpy(vertex_buf + 32 * 3, texcoords_flipped_rotated, 8 * sizeof(GLfloat));
			break;
	}

	glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buf), vertex_buf, GL_STREAM_DRAW);
}

static int32_t ps3graphics_psgl_init(uint32_t scaleEnable, uint32_t scaleFactor)
{
	glDisable(GL_DEPTH_TEST);
	ps3graphics_set_vsync(m_vsync);

	if(scaleEnable)
		ps3graphics_set_fbo_scale(scaleEnable, scaleFactor);

	ps3graphics_init_cg();

	SetViewports();

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glColor4f(1.0, 1.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	psglSwap();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glGenBuffers(2, vbo);
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, vbo[0]);
	glBufferData(GL_TEXTURE_REFERENCE_BUFFER_SCE, m_height * m_pitch, NULL, GL_STREAM_DRAW);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	ps3graphics_set_smooth(m_smooth, 0);
	ps3graphics_set_smooth(m_smooth2, 1);

	/* PSGL doesn't clear the screen on startup, so let's do that here.*/
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glColor4f(1.0, 1.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	psglSwap();

	SetScreenDimensions(m_width, m_height, m_width, 0);

	ps3graphics_set_orientation(NORMAL);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, 0);
	glTexCoordPointer(2, GL_FLOAT, 0, (void*)128);

	return CELL_OK;
}

static void ps3graphics_get_all_available_resolutions()
{
	bool defaultresolution = true;
	uint32_t videomode[] = {
		CELL_VIDEO_OUT_RESOLUTION_480, CELL_VIDEO_OUT_RESOLUTION_576,
		CELL_VIDEO_OUT_RESOLUTION_960x1080, CELL_VIDEO_OUT_RESOLUTION_720,
		CELL_VIDEO_OUT_RESOLUTION_1280x1080, CELL_VIDEO_OUT_RESOLUTION_1440x1080,
		CELL_VIDEO_OUT_RESOLUTION_1600x1080, CELL_VIDEO_OUT_RESOLUTION_1080};

	/* Provide future expandability of the videomode array*/
	uint16_t num_videomodes = sizeof(videomode)/sizeof(uint32_t);
	for (int i=0; i < num_videomodes; i++) {
		if (ps3graphics_check_resolution(videomode[i]))
		{
			m_supportedResolutions.push_back(videomode[i]);
			m_initialResolution = videomode[i];
			if (m_currentResolutionId == videomode[i])
			{
				defaultresolution = false;
				m_currentResolutionPos = m_supportedResolutions.size()-1;
			}
		}
	}

	/* In case we didn't specify a resolution - make the last resolution*/
	/* that was added to the list (the highest resolution) the default resolution*/
	if (m_currentResolutionPos > num_videomodes | defaultresolution)
		m_currentResolutionPos = m_supportedResolutions.size()-1;
}

static void ps3graphics_set_resolution()
{
	cellVideoOutGetState(CELL_VIDEO_OUT_PRIMARY, 0, &m_stored_video_state);
}

static void ps3graphics_init(uint32_t scaleEnabled, uint32_t scaleFactor)
{
	ps3graphics_psgl_init_device(m_resolutionId, m_pal60Hz, m_tripleBuffering);
	int32_t ret = ps3graphics_psgl_init(scaleEnabled, scaleFactor);

	if (ret == CELL_OK)
	{
		ps3graphics_get_all_available_resolutions();
		ps3graphics_set_resolution();
	}
}


void ps3graphics_new(uint32_t resolution, uint32_t aspect, uint32_t smooth, uint32_t smooth2, const char * shader, const char * shader2, const char * menu_shader, uint32_t overscan, float overscan_amount, uint32_t pal60Hz, uint32_t vsync, uint32_t tripleBuffering, uint32_t viewport_x, uint32_t viewport_y, uint32_t viewport_width, uint32_t viewport_height, uint32_t scale_enabled, uint32_t scale_factor)
{
	/*psglgraphics*/
	psgl_device = NULL;
	psgl_context = NULL;
	gl_width = 0;
	gl_height = 0;

	m_smooth = smooth;
	m_smooth2 = smooth2;
	ps3graphics_check_resolution(resolution) ? m_resolutionId = resolution : m_resolutionId = 0;

	ps3graphics_set_overscan(overscan, overscan_amount, 0);
	m_pal60Hz = pal60Hz;
	m_vsync = vsync;
	m_tripleBuffering =  tripleBuffering;

	strcpy(curFragmentShaderPath[0], shader); 
	strcpy(curFragmentShaderPath[1], shader2); 
	strcpy(curFragmentShaderPath[2], menu_shader); 

	decode_buffer = (uint8_t*)memalign(128, 2048 * 2048 * 4);
	memset(decode_buffer, 0, (2048 * 2048 * 4));

	init_fbo_memb();

	m_width = 240;
	m_height = 160;
	ps3graphics_set_aspect_ratio(aspect, m_width, m_height, 0);

	m_viewport_x = viewport_x;
	m_viewport_y = viewport_y;
	m_viewport_width = viewport_width;
	m_viewport_height = viewport_height;
	m_calculate_aspect_ratio_before_game_load = 1;

	setup_aspect_ratio_array();
	ps3graphics_init(scale_enabled, scale_factor);
}

const char * ps3graphics_get_fragment_shader_path(unsigned index)
{
	return curFragmentShaderPath[index];
}


static void ps3graphics_psgl_deinit_device()
{
	glFinish();
	cellDbgFontExit();

	psglDestroyContext(psgl_context);
	psglDestroyDevice(psgl_device);
#if CELL_SDK_VERSION == 0x340001
	/*FIXME: It will crash here for 1.92 - termination of the PSGL library - works fine for 3.41*/
	psglExit();
#else
	/*for 1.92*/
	gl_width = 0;
	gl_height = 0;
	psgl_context = NULL;
	psgl_device = NULL;
#endif
}

static void ps3graphics_deinit()
{
	glDeleteTextures(1, &tex);
	ps3graphics_psgl_deinit_device();
}

static void ps3graphics_destroy()
{
	ps3graphics_deinit();
	free(decode_buffer);

	#if 0
	if (tracker)
	{
		snes_tracker_free(tracker);

		for (std::vector<lookup_texture>::iterator itr = lut_textures.begin();
				itr != lut_textures.end(); ++itr)
		{
			glDeleteTextures(1, &itr->tex);
		}
	}
	#endif
}

void ps3graphics_set_fbo_scale(uint32_t enable, unsigned scale)
{
	if (scale == 0)
		scale = 1;

	if (fbo && enable)
	{
		glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glDeleteTextures(1, &fbo_tex);
		glDeleteFramebuffersOES(1, &fbo);
		fbo = 0;
		fbo_tex = 0;
	}

	fbo_enable = enable;
	fbo_scale = scale;

	if (enable)
	{
		fbo_width = fbo_height = 256 * scale;
		glGenFramebuffersOES(1, &fbo);
		glGenTextures(1, &fbo_tex);

		glBindTexture(GL_TEXTURE_2D, fbo_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, SCREEN_RENDER_PIXEL_FORMAT, fbo_width, fbo_height, 0, SCREEN_RENDER_PIXEL_FORMAT, GL_UNSIGNED_INT_8_8_8_8, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		glBindFramebufferOES(GL_FRAMEBUFFER_OES, fbo);
		glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, fbo_tex, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
	}
}

/******************************************************************************* 
	libdbgfont macros
********************************************************************************/
#ifdef CELL_DEBUG_FPS
#define write_fps() \
	static uint64_t last_time = 0; \
	static uint64_t last_fps = 0; \
	static uint64_t frames = 0; \
	static float fps = 0.0; \
	\
	frames++; \
	\
	if (frames == 100) \
	{ \
		uint64_t new_time = sys_time_get_system_time(); \
		uint64_t delta = new_time - last_time; \
		last_time = new_time; \
		frames = 0; \
		fps = 100000000.0 / delta; \
	} \
	\
	dprintf_noswap(0.02, 0.02, 1.0, "FPS: %.2f\n", fps);
#else
#define write_fps()
#endif

/******************************************************************************* 
	libdbgfont
********************************************************************************/

void ps3graphics_init_dbgfont()
{
	CellDbgFontConfig cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.bufSize = 512;
	cfg.screenWidth = gl_width;
	cfg.screenHeight = gl_height;
	cellDbgFontInit(&cfg);
}

#ifdef CELL_DEBUG_FPS
static void dprintf_noswap(float x, float y, float scale, const char* fmt, ...)
{
	char buffer[512];

	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buffer, 512, fmt, ap);
	cellDbgFontPuts(x, y, scale, 0xffffffff, buffer);
	va_end(ap);

	cellDbgFontDraw();
}
#endif

/******************************************************************************* 
	Draw macros
********************************************************************************/

#define texture_backdrop(number) \
	if (tex_backdrop) \
	{ \
		/* Set up texture coord array (TEXCOORD1). */ \
		glClientActiveTexture(GL_TEXTURE1); \
		glEnableClientState(GL_TEXTURE_COORD_ARRAY); \
		glTexCoordPointer(2, GL_FLOAT, 0, (void*)(128 * 3)); \
		glClientActiveTexture(GL_TEXTURE0); \
		\
		CGparameter param = cgGetNamedParameter(_fragmentProgram[number], "bg"); \
		cgGLSetTextureParameter(param, tex_backdrop); \
		cgGLEnableTextureParameter(param); \
	}

#define UpdateCgParams(width, height, tex_width, tex_height, index) \
	cgGLSetStateMatrixParameter(_cgpModelViewProj[index], CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY); \
	cgGLSetParameter2f(_cgpVideoSize[index], width, height); \
	cgGLSetParameter2f(_cgpTextureSize[index], width, height); \
	cgGLSetParameter2f(_cgpOutputSize[index], tex_width, tex_height); \
	cgGLSetParameter2f(_cgp_vertex_VideoSize[index], width, height); \
	cgGLSetParameter2f(_cgp_vertex_TextureSize[index], width, height); \
	cgGLSetParameter2f(_cgp_vertex_OutputSize[index], tex_width, tex_height); \
	cgGLSetParameter1f(_cgp_timer[index], frame_count); \
	cgGLSetParameter1f(_cgp_vertex_timer[index], frame_count);

#define UpdateCgParams_Shader2(fbo_vp_width, fbo_vp_height, tex_width, tex_height, cgview_width, cgview_height, index) \
	cgGLSetStateMatrixParameter(_cgpModelViewProj[index], CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY); \
	cgGLSetParameter2f(_cgpVideoSize[index], fbo_vp_width, fbo_vp_height); \
	cgGLSetParameter2f(_cgpTextureSize[index], tex_width, tex_height); \
	cgGLSetParameter2f(_cgpOutputSize[index], _cgViewWidth, _cgViewHeight); \
	cgGLSetParameter2f(_cgp_vertex_VideoSize[index], fbo_vp_width, fbo_vp_height); \
	cgGLSetParameter2f(_cgp_vertex_TextureSize[index], tex_width, tex_height); \
	cgGLSetParameter2f(_cgp_vertex_OutputSize[index], cgview_width, cgview_height); \
	cgGLSetParameter1f(_cgp_timer[index], frame_count); \
	cgGLSetParameter1f(_cgp_vertex_timer[index], frame_count);

/******************************************************************************* 
	Draw functions
********************************************************************************/

#if 0
/* Set SNES state uniforms.*/
static void ps3graphics_update_state_uniforms(unsigned index)
{
	if (tracker)
	{
		struct snes_tracker_uniform info[64];
		unsigned cnt = snes_get_uniform(tracker, info, 64, frame_count);
		for (unsigned i = 0; i < cnt; i++)
		{
			CGparameter param_v = cgGetNamedParameter(_vertexProgram[index], info[i].id);
			CGparameter param_f = cgGetNamedParameter(_fragmentProgram[index], info[i].id);
			cgGLSetParameter1f(param_v, info[i].value);
			cgGLSetParameter1f(param_f, info[i].value);
		}

		for (std::vector<lookup_texture>::const_iterator itr = lut_textures.begin(); itr != lut_textures.end(); ++itr)
		{
			CGparameter param = cgGetNamedParameter(_fragmentProgram[index], itr->id.c_str());
			cgGLSetTextureParameter(param, itr->tex);
			cgGLEnableTextureParameter(param);
		}
	}
}
#endif

void ps3graphics_draw(uint8_t *XBuf)
{
	frame_count++;
	if(fbo_enable)
	{
		glBindFramebufferOES(GL_FRAMEBUFFER_OES, fbo);
		glClear(GL_COLOR_BUFFER_BIT);

		glBindTexture(GL_TEXTURE_2D, tex);
		glBufferSubData(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0, m_height * m_pitch, XBuf);
		glTextureReferenceSCE(GL_TEXTURE_2D, 1, m_width, m_height, 0, SCREEN_RENDER_PIXEL_FORMAT, m_pitch, 0);


		glViewport(0, 0, fbo_vp_width, fbo_vp_height);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrthof(0, 1, 0, 1, -1, 1);
		glMatrixMode(GL_MODELVIEW);

		UpdateCgParams(m_width, m_height, fbo_vp_width, fbo_vp_height, 0);

		texture_backdrop(0);

		glDrawArrays(GL_QUADS, 0, 4);

		glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
		glBindTexture(GL_TEXTURE_2D, fbo_tex);

		SetViewports();

		cgGLBindProgram(_vertexProgram[1]);
		cgGLBindProgram(_fragmentProgram[1]);

		UpdateCgParams_Shader2(fbo_vp_width, fbo_vp_height, fbo_width, fbo_height, _cgViewWidth, _cgViewHeight, 1);

		texture_backdrop(1);

		glClear(GL_COLOR_BUFFER_BIT);
		GLfloat xamt = (GLfloat)fbo_vp_width / fbo_width;
		GLfloat yamt = (GLfloat)fbo_vp_height / fbo_height;
		glClear(GL_COLOR_BUFFER_BIT);

		GLfloat fbo_tex_coord[8] = {0};
		glTexCoordPointer(2, GL_FLOAT, 0, (void*)256);

		fbo_tex_coord[3] = yamt;
		fbo_tex_coord[4] = xamt;
		fbo_tex_coord[5] = yamt;
		fbo_tex_coord[6] = xamt;

		glBufferSubData(GL_ARRAY_BUFFER, 256, sizeof(fbo_tex_coord), fbo_tex_coord);
		glDrawArrays(GL_QUADS, 0, 4);
		glTexCoordPointer(2, GL_FLOAT, 0, (void*)128);

		cgGLBindProgram(_vertexProgram[0]);
		cgGLBindProgram(_fragmentProgram[0]);
		write_fps();

		glBindTexture(GL_TEXTURE_2D, tex);
	}
	else
	{
		glClear(GL_COLOR_BUFFER_BIT);

		glBufferSubData(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0, m_height * m_pitch, XBuf);
		glTextureReferenceSCE(GL_TEXTURE_2D, 1, m_width, m_height, 0, SCREEN_RENDER_PIXEL_FORMAT, m_pitch, 0);
		UpdateCgParams(m_width, m_height, m_width, m_height, 0);

		texture_backdrop(0);

		glDrawArrays(GL_QUADS, 0, 4);
		write_fps();
	}
}

void ps3graphics_draw_menu(void)
{
	frame_count++;
	GLuint temp_width = gl_width;
	GLuint temp_height = gl_height;
	_cgViewWidth = temp_width;
	_cgViewHeight = temp_width;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glViewport(0, 0, temp_width, temp_height);
	glOrthof(0, 1, 0, 1, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	cgGLBindProgram(_vertexProgram[MENU_SHADER_NO]);
	cgGLBindProgram(_fragmentProgram[MENU_SHADER_NO]);
	cgGLSetStateMatrixParameter(_cgpModelViewProj[MENU_SHADER_NO], CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY);
	cgGLSetParameter2f(_cgpVideoSize[MENU_SHADER_NO], temp_width, temp_height);
	cgGLSetParameter2f(_cgpTextureSize[MENU_SHADER_NO], temp_width, temp_height);
	cgGLSetParameter2f(_cgpOutputSize[MENU_SHADER_NO], _cgViewWidth, _cgViewHeight);
	cgGLSetParameter2f(_cgp_vertex_VideoSize[MENU_SHADER_NO], temp_width, temp_height);
	cgGLSetParameter2f(_cgp_vertex_TextureSize[MENU_SHADER_NO], temp_width, temp_height);
	cgGLSetParameter2f(_cgp_vertex_OutputSize[MENU_SHADER_NO], _cgViewWidth, _cgViewHeight);

	_cgp_timer[MENU_SHADER_NO] = cgGetNamedParameter(_fragmentProgram[MENU_SHADER_NO], "IN.frame_count");
	_cgp_vertex_timer[MENU_SHADER_NO] = cgGetNamedParameter(_vertexProgram[MENU_SHADER_NO], "IN.frame_count");
	cgGLSetParameter1f(_cgp_timer[MENU_SHADER_NO], frame_count);
	cgGLSetParameter1f(_cgp_vertex_timer[MENU_SHADER_NO], frame_count);
	CGparameter param = cgGetNamedParameter(_fragmentProgram[MENU_SHADER_NO], "bg");
	cgGLSetTextureParameter(param, tex_menu);
	cgGLEnableTextureParameter(param);

	/* Set up texture coord array (TEXCOORD1).*/
	glClientActiveTexture(GL_TEXTURE1);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, (void*)(128 * 3));
	glClientActiveTexture(GL_TEXTURE0);

	glDrawArrays(GL_QUADS, 0, 4); 

	/* EnableTextureParameter might overwrite bind in TEXUNIT0.*/
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	cgGLBindProgram(_vertexProgram[0]);
	cgGLBindProgram(_fragmentProgram[0]);
}

void ps3graphics_resize_aspect_mode_input_loop(uint64_t state)
{
	if(CTRL_LSTICK_LEFT(state) || CTRL_LEFT(state))
		m_viewport_x -= 1;
	else if (CTRL_LSTICK_RIGHT(state) || CTRL_RIGHT(state))
		m_viewport_x += 1;
	if (CTRL_LSTICK_UP(state) || CTRL_UP(state))
		m_viewport_y += 1;
	else if (CTRL_LSTICK_DOWN(state) || CTRL_DOWN(state)) 
		m_viewport_y -= 1;

	if (CTRL_RSTICK_LEFT(state) || CTRL_L1(state))
		m_viewport_width -= 1;
	else if (CTRL_RSTICK_RIGHT(state) || CTRL_R1(state))
		m_viewport_width += 1;
	if (CTRL_RSTICK_UP(state) || CTRL_L2(state))
		m_viewport_height += 1;
	else if (CTRL_RSTICK_DOWN(state) || CTRL_R2(state))
		m_viewport_height -= 1;

	if (CTRL_TRIANGLE(state))
	{
		m_viewport_x = m_viewport_y = 0;
		m_viewport_width = gl_width;
		m_viewport_height = gl_height;
	}

	CalculateViewports();
	SetViewports();
#define x_position 0.3f
#define font_size 1.1f
#define ypos 0.19f
#define ypos_increment 0.04f
	cellDbgFontPrintf	(x_position,	ypos,	font_size+0.01f,	BLUE,	"Viewport X: #%d", m_viewport_x);
	cellDbgFontPrintf	(x_position,	ypos,	font_size,	GREEN,	"Viewport X: #%d", m_viewport_x);

	cellDbgFontPrintf	(x_position,	ypos+(ypos_increment*1),	font_size+0.01f,	BLUE,	"Viewport Y: #%d", m_viewport_y);
	cellDbgFontPrintf	(x_position,	ypos+(ypos_increment*1),	font_size,	GREEN,	"Viewport Y: #%d", m_viewport_y);

	cellDbgFontPrintf	(x_position,	ypos+(ypos_increment*2),	font_size+0.01f,	BLUE,	"Viewport Width: #%d", m_viewport_width);
	cellDbgFontPrintf	(x_position,	ypos+(ypos_increment*2),	font_size,	GREEN,	"Viewport Width: #%d", m_viewport_width);

	cellDbgFontPrintf	(x_position,	ypos+(ypos_increment*3),	font_size+0.01f,	BLUE,	"Viewport Height: #%d", m_viewport_height);
	cellDbgFontPrintf	(x_position,	ypos+(ypos_increment*3),	font_size,	GREEN,	"Viewport Height: #%d", m_viewport_height);

	cellDbgFontPrintf (0.09f,   0.40f,   font_size+0.01f,      BLUE,          "CONTROLS:");
	cellDbgFontPrintf (0.09f,   0.40f,   font_size,      LIGHTBLUE,           "CONTROLS:");

	cellDbgFontPrintf (0.09f,   0.46f,   font_size+0.01f,      BLUE,          "LEFT / LSTICK UP             - Decrement Viewport X");
	cellDbgFontPrintf (0.09f,   0.46f,   font_size,      LIGHTBLUE,           "LEFT / LSTICK UP             - Decrement Viewport X");

	cellDbgFontDraw();

	cellDbgFontPrintf (0.09f,   0.48f,   font_size+0.01f,      BLUE,          "RIGHT / LSTICK RIGHT         - Increment Viewport X");
	cellDbgFontPrintf (0.09f,   0.48f,   font_size,      LIGHTBLUE,           "RIGHT / LSTICK RIGHT         - Increment Viewport X");

	cellDbgFontPrintf (0.09f,   0.50f,   font_size+0.01f,      BLUE,          "UP / LSTICK UP               - Increment Viewport Y");
	cellDbgFontPrintf (0.09f,   0.50f,   font_size,      LIGHTBLUE,           "UP / LSTICK UP               - Increment Viewport Y");

	cellDbgFontDraw();

	cellDbgFontPrintf (0.09f,   0.52f,   font_size+0.01f,      BLUE,          "DOWN / LSTICK DOWN           - Decrement Viewport Y");
	cellDbgFontPrintf (0.09f,   0.52f,   font_size,      LIGHTBLUE,           "DOWN / LSTICK DOWN           - Decrement Viewport Y");

	cellDbgFontPrintf (0.09f,   0.54f,   font_size+0.01f,      BLUE,          "L1 / RSTICK LEFT             - Decrement Viewport Width");
	cellDbgFontPrintf (0.09f,   0.54f,   font_size,      LIGHTBLUE,           "L1 / RSTICK LEFT             - Decrement Viewport Width");

	cellDbgFontDraw();

	cellDbgFontPrintf (0.09f,   0.56f,   font_size+0.01f,      BLUE,          "R1/RSTICK RIGHT              - Increment Viewport Width");
	cellDbgFontPrintf (0.09f,   0.56f,   font_size,      LIGHTBLUE,           "R1/RSTICK RIGHT              - Increment Viewport Width");

	cellDbgFontPrintf (0.09f,   0.58f,   font_size+0.01f,      BLUE,          "L2 / RSTICK UP               - Increment Viewport Height");
	cellDbgFontPrintf (0.09f,   0.58f,   font_size,      LIGHTBLUE,           "L2 / RSTICK UP               - Increment Viewport Height");

	cellDbgFontDraw();

	cellDbgFontPrintf (0.09f,   0.60f,   font_size+0.01f,      BLUE,          "R2 / RSTICK DOWN             - Decrement Viewport Height");
	cellDbgFontPrintf (0.09f,   0.60f,   font_size,      LIGHTBLUE,           "R2 / RSTICK DOWN             - Decrement Viewport Height");

	cellDbgFontPrintf (0.09f,   0.66f,   font_size+0.01f,      BLUE,          "TRIANGLE                     - Reset To Defaults");
	cellDbgFontPrintf (0.09f,   0.66f,   font_size,      LIGHTBLUE,           "TRIANGLE                     - Reset To Defaults");

	cellDbgFontPrintf (0.09f,   0.68f,   font_size+0.01f,      BLUE,          "CIRCLE                       - Return to Ingame Menu");
	cellDbgFontPrintf (0.09f,   0.68f,   font_size,      LIGHTBLUE,           "CIRCLE                       - Return to Ingame Menu");

	cellDbgFontDraw();

	cellDbgFontPrintf (0.09f,   0.90f,   0.91f+0.01f,      BLUE,           "Allows you to resize the screen by moving around the two analog sticks.\nPress TRIANGLE to reset to default values, and CIRCLE to go back to the\nin-game menu.");
	cellDbgFontPrintf (0.09f,   0.90f,   0.91f,      LIGHTBLUE,           "Allows you to resize the screen by moving around the two analog sticks.\nPress TRIANGLE to reset to default values, and CIRCLE to go back to the\nin-game menu.");
	cellDbgFontDraw();
}

/******************************************************************************* 
	Resolution functions
********************************************************************************/

void ps3graphics_change_resolution(uint32_t resId, uint16_t pal60Hz, uint16_t tripleBuffering, uint32_t scaleEnabled, uint32_t scaleFactor)
{
	cellDbgFontExit();
	ps3graphics_psgl_deinit_device();

	ps3graphics_psgl_init_device(resId, pal60Hz, tripleBuffering);
	ps3graphics_psgl_init(scaleEnabled, scaleFactor);
	ps3graphics_init_dbgfont();
	ps3graphics_set_resolution();
}

int ps3graphics_check_resolution(uint32_t resId)
{
	return cellVideoOutGetResolutionAvailability(CELL_VIDEO_OUT_PRIMARY, resId, CELL_VIDEO_OUT_ASPECT_AUTO,0);
}

void ps3graphics_next_resolution()
{
	if(m_currentResolutionPos+1 < m_supportedResolutions.size())
	{
		m_currentResolutionPos++;
		m_currentResolutionId = m_supportedResolutions[m_currentResolutionPos];
	}
}

void ps3graphics_previous_resolution()
{
	if(m_currentResolutionPos > 0)
	{
		m_currentResolutionPos--;
		m_currentResolutionId = m_supportedResolutions[m_currentResolutionPos];
	}
}

void ps3graphics_switch_resolution(uint32_t resId, uint16_t pal60Hz, uint16_t tripleBuffering, uint32_t scaleEnabled, uint32_t scaleFactor)
{
	if(ps3graphics_check_resolution(resId))
	{
		ps3graphics_change_resolution(resId, pal60Hz, tripleBuffering, scaleEnabled, scaleFactor);
		ps3graphics_set_fbo_scale(scaleEnabled, scaleFactor);
	}
	ps3graphics_load_fragment_shader(DEFAULT_MENU_SHADER_FILE, 2);
}

/******************************************************************************* 
	Cg
********************************************************************************/


static CGprogram load_shader_from_source(CGcontext cgtx, CGprofile target, const char* filename, const char* entry)
{
	const char* args[] = { "-fastmath", "-unroll=all", "-ifcvt=all", 0 };
	CGprogram id = cgCreateProgramFromFile(cgtx, CG_SOURCE, filename, target, entry, args);

	return id;
}

int32_t ps3graphics_load_fragment_shader(const char * shaderPath, unsigned index)
{
	/* store the current path*/
	strcpy(curFragmentShaderPath[index], shaderPath);

	_vertexProgram[index] = load_shader_from_source(_cgContext, CG_PROFILE_SCE_VP_RSX, shaderPath, "main_vertex");
	_fragmentProgram[index] = load_shader_from_source(_cgContext, CG_PROFILE_SCE_FP_RSX, shaderPath, "main_fragment");

	/* bind and enable the vertex and fragment programs*/
	cgGLEnableProfile(CG_PROFILE_SCE_VP_RSX);
	cgGLEnableProfile(CG_PROFILE_SCE_FP_RSX);
	cgGLBindProgram(_vertexProgram[index]);
	cgGLBindProgram(_fragmentProgram[index]);

	/* acquire mvp param from v shader*/
	_cgpModelViewProj[index] = cgGetNamedParameter(_vertexProgram[index], "modelViewProj");
	_cgpVideoSize[index] = cgGetNamedParameter(_fragmentProgram[index], "IN.video_size");
	_cgpTextureSize[index] = cgGetNamedParameter(_fragmentProgram[index], "IN.texture_size");
	_cgpOutputSize[index] = cgGetNamedParameter(_fragmentProgram[index], "IN.output_size");
	_cgp_vertex_VideoSize[index] = cgGetNamedParameter(_vertexProgram[index], "IN.video_size");
	_cgp_vertex_TextureSize[index] = cgGetNamedParameter(_vertexProgram[index], "IN.texture_size");
	_cgp_vertex_OutputSize[index] = cgGetNamedParameter(_vertexProgram[index], "IN.output_size");

	_cgp_timer[index] = cgGetNamedParameter(_fragmentProgram[index], "IN.frame_count");
	_cgp_vertex_timer[index] = cgGetNamedParameter(_vertexProgram[index], "IN.frame_count");

	if (index == 1 && _vertexProgram[0] && _fragmentProgram[0])
	{
		cgGLBindProgram(_vertexProgram[0]);
		cgGLBindProgram(_fragmentProgram[0]);
	}

	return CELL_OK;
}

/******************************************************************************* 
	Get functions
********************************************************************************/

uint32_t ps3graphics_get_initial_resolution (void)
{
	return m_initialResolution;
}

uint32_t ps3graphics_get_pal60hz()
{
	return m_pal60Hz;
}

uint32_t ps3graphics_get_current_resolution()
{
	return m_supportedResolutions[m_currentResolutionPos];
}

uint32_t ps3graphics_get_viewport_x(void)
{
	return m_viewport_x;
}

uint32_t ps3graphics_get_viewport_y(void)
{
	return m_viewport_y;
}

uint32_t ps3graphics_get_viewport_width(void)
{
	return m_viewport_width;
}

uint32_t ps3graphics_get_viewport_height(void)
{
	return m_viewport_height;
}

int ps3graphics_get_aspect_ratio_int(bool orientation)
{
	/*orientation true is aspect_y, false is aspect_x*/
	if(orientation)
		return aspect_y;
	else
		return aspect_x;
}

float ps3graphics_get_aspect_ratio_float(int enum_)
{
	return aspectratios[enum_];
}

const char * ps3graphics_get_resolution_label(uint32_t resolution)
{
	switch(resolution)
	{
		case CELL_VIDEO_OUT_RESOLUTION_480:
			return  "720x480 (480p)";
		case CELL_VIDEO_OUT_RESOLUTION_576:
			return "720x576 (576p)"; 
		case CELL_VIDEO_OUT_RESOLUTION_720:
			return "1280x720 (720p)";
		case CELL_VIDEO_OUT_RESOLUTION_960x1080:
			return "960x1080";
		case CELL_VIDEO_OUT_RESOLUTION_1280x1080:
			return "1280x1080";
		case CELL_VIDEO_OUT_RESOLUTION_1440x1080:
			return "1440x1080";
		case CELL_VIDEO_OUT_RESOLUTION_1600x1080:
			return "1600x1080";
		case CELL_VIDEO_OUT_RESOLUTION_1080:
			return "1920x1080 (1080p)";
	}
}

CellVideoOutState ps3graphics_get_video_out_state()
{
	return m_stored_video_state;
}

/******************************************************************************* 
	Set functions
********************************************************************************/

void ps3graphics_set_overscan(uint32_t will_overscan, float amount, uint32_t setviewport)
{
	m_overscan_amount = amount;
	m_overscan = will_overscan;


	CalculateViewports();
	if(setviewport)
	{
		SetViewports();
	}
}

void ps3graphics_set_pal60hz(uint32_t pal60Hz)
{
	m_pal60Hz = pal60Hz;
}

void ps3graphics_set_triple_buffering(uint32_t triple_buffering)
{
	m_tripleBuffering = triple_buffering;
}

void ps3graphics_set_smooth(uint32_t smooth, unsigned index)
{
	if (index == 0)
	{
		m_smooth = smooth;
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_smooth ? GL_LINEAR : GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_smooth ? GL_LINEAR : GL_NEAREST);
	}
	else if (fbo_tex)
	{
		glBindTexture(GL_TEXTURE_2D, fbo_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, tex);
	}
}

void ps3graphics_set_aspect_ratio(uint32_t aspect, uint32_t width, uint32_t height, uint32_t setviewport)
{
	m_calculate_aspect_ratio_before_game_load = 0;
	switch(aspect)
	{
		case ASPECT_RATIO_1_1:
			m_ratio = SCREEN_1_1_ASPECT_RATIO;
			aspect_x = 1;
			aspect_y = 1;
			break;
		case ASPECT_RATIO_4_3:
			m_ratio = SCREEN_4_3_ASPECT_RATIO;
			aspect_x = 4;
			aspect_y = 3;
			break;
		case ASPECT_RATIO_5_4:
			m_ratio = SCREEN_5_4_ASPECT_RATIO;
			aspect_x = 5;
			aspect_y = 4;
			break;
		case ASPECT_RATIO_8_7:
			m_ratio = SCREEN_8_7_ASPECT_RATIO;
			aspect_x = 8;
			aspect_y = 7;
			break;
		case ASPECT_RATIO_16_9:
			m_ratio = SCREEN_16_9_ASPECT_RATIO;
			aspect_x = 16;
			aspect_y = 9;
			break;
		case ASPECT_RATIO_16_10:
			m_ratio = SCREEN_16_10_ASPECT_RATIO;
			aspect_x = 16;
			aspect_y = 10;
			break;
		case ASPECT_RATIO_16_15:
			m_ratio = SCREEN_16_15_ASPECT_RATIO;
			aspect_x = 16;
			aspect_y = 15;
			break;
		case ASPECT_RATIO_19_14:
			m_ratio = SCREEN_19_14_ASPECT_RATIO;
			aspect_x = 19;
			aspect_y = 14;
			break;
		case ASPECT_RATIO_2_1:
			m_ratio = SCREEN_2_1_ASPECT_RATIO;
			aspect_x = 2;
			aspect_y = 1;
			break;
		case ASPECT_RATIO_3_2:
			m_ratio = SCREEN_3_2_ASPECT_RATIO;
			aspect_x = 3;
			aspect_y = 2;
			break;
		case ASPECT_RATIO_3_4:
			m_ratio = SCREEN_3_4_ASPECT_RATIO;
			aspect_x = 3;
			aspect_y = 4;
			break;
		case ASPECT_RATIO_AUTO:
			m_calculate_aspect_ratio_before_game_load = 1;
			if(width != 0 && height != 0)
			{
				unsigned len, highest, i;

				len = width < height ? width : height;
				highest = 1;
				for ( i = 1; i < len; i++)
				{
					if ((width % i) == 0 && (height % i) == 0)
						highest = i;
				}

				aspect_x = width / highest;
				aspect_y = height / highest;
				m_ratio = ((float)aspect_x)/((float)aspect_y);
				aspectratios[ASPECT_RATIO_AUTO] = m_ratio;
			}
			break;
		case ASPECT_RATIO_CUSTOM:
			m_ratio = SCREEN_CUSTOM_ASPECT_RATIO;
			aspect_x = 0;
			aspect_y = 1;
			break;
	}

	CalculateViewports();

	if(setviewport)
	{
		SetViewports();
	}
}

void ps3graphics_set_dimensions(unsigned width, unsigned height, unsigned pitch)
{
	SetScreenDimensions(width, height, pitch, 1);
	fbo_vp_width = m_width * fbo_scale;
	fbo_vp_height = m_height * fbo_scale;
}

void ps3graphics_set_vsync(uint32_t vsync)
{
	vsync ? glEnable(GL_VSYNC_SCE) : glDisable(GL_VSYNC_SCE);
}

uint32_t ps3graphics_set_text_message_speed(uint32_t value)
{
	return frame_count + value;
}

/******************************************************************************* 
	Image decompression
********************************************************************************/

/******************************************************************************* 
	Image decompression - structs
********************************************************************************/

typedef struct CtrlMallocArg
{
	uint32_t mallocCallCounts;
} CtrlMallocArg;

typedef struct CtrlFreeArg
{
	uint32_t freeCallCounts;
} CtrlFreeArg;

void * img_malloc(uint32_t size, void * a)
{
	CtrlMallocArg *arg;
	arg = (CtrlMallocArg *) a;
	arg->mallocCallCounts++;
	return malloc(size);
}

static int img_free(void *ptr, void * a)
{
	CtrlFreeArg *arg;
	arg = (CtrlFreeArg *) a;
	arg->freeCallCounts++;
	free(ptr);
	return 0;
}

/******************************************************************************* 
	Image decompression - libJPEG
********************************************************************************/

static bool ps3graphics_load_jpeg(const char * path, unsigned &width, unsigned &height, uint8_t *data)
{
	CtrlMallocArg              MallocArg;
	CtrlFreeArg                FreeArg;
	CellJpgDecMainHandle       mHandle = NULL;
	CellJpgDecSubHandle        sHandle = NULL;
	CellJpgDecThreadInParam    InParam;
	CellJpgDecThreadOutParam   OutParam;
	CellJpgDecSrc              src;
	CellJpgDecOpnInfo          opnInfo;
	CellJpgDecInfo             info;
	CellJpgDecInParam          inParam;
	CellJpgDecOutParam         outParam;
	CellJpgDecDataOutInfo      dOutInfo;
	CellJpgDecDataCtrlParam    dCtrlParam;

	MallocArg.mallocCallCounts = 0;
	FreeArg.freeCallCounts = 0;
	InParam.spuThreadEnable = CELL_JPGDEC_SPU_THREAD_ENABLE;
	InParam.ppuThreadPriority = 1001;
	InParam.spuThreadPriority = 250;
	InParam.cbCtrlMallocFunc = img_malloc;
	InParam.cbCtrlMallocArg = &MallocArg;
	InParam.cbCtrlFreeFunc = img_free;
	InParam.cbCtrlFreeArg = &FreeArg;

	int ret_jpeg, ret = -1;
	ret_jpeg = cellJpgDecCreate(&mHandle, &InParam, &OutParam);

	if (ret_jpeg != CELL_OK)
	{
		goto error;
	}

	memset(&src, 0, sizeof(CellJpgDecSrc));
	src.srcSelect        = CELL_JPGDEC_FILE;
	src.fileName         = path;
	src.fileOffset       = 0;
	src.fileSize         = 0;
	src.streamPtr        = NULL;
	src.streamSize       = 0;

	src.spuThreadEnable  = CELL_JPGDEC_SPU_THREAD_ENABLE;

	ret = cellJpgDecOpen(mHandle, &sHandle, &src, &opnInfo);

	if (ret != CELL_OK)
	{
		goto error;
	}

	ret = cellJpgDecReadHeader(mHandle, sHandle, &info);

	if (ret != CELL_OK)
	{
		goto error;
	}

	inParam.commandPtr         = NULL;
	inParam.method             = CELL_JPGDEC_FAST;
	inParam.outputMode         = CELL_JPGDEC_TOP_TO_BOTTOM;
	inParam.outputColorSpace   = CELL_JPG_ARGB;
	inParam.downScale          = 1;
	inParam.outputColorAlpha = 0xfe;
	ret = cellJpgDecSetParameter(mHandle, sHandle, &inParam, &outParam);

	if (ret != CELL_OK)
	{
		sys_process_exit(0);
		goto error;
	}

	dCtrlParam.outputBytesPerLine = outParam.outputWidth * 4;
	ret = cellJpgDecDecodeData(mHandle, sHandle, data, &dCtrlParam, &dOutInfo);

	if (ret != CELL_OK || dOutInfo.status != CELL_JPGDEC_DEC_STATUS_FINISH)
	{
		sys_process_exit(0);
		goto error;
	}

	width = outParam.outputWidth;
	height = outParam.outputHeight;

	cellJpgDecClose(mHandle, sHandle);
	cellJpgDecDestroy(mHandle);

	return true;

error:
	if (mHandle && sHandle)
		cellJpgDecClose(mHandle, sHandle);
	if (mHandle)
		cellJpgDecDestroy(mHandle);
	return false;
}

/******************************************************************************* 
	Image decompression - libPNG
********************************************************************************/

static bool ps3graphics_load_png(const char * path, unsigned &width, unsigned &height, uint8_t *data)
{
	CtrlMallocArg              MallocArg;
	CtrlFreeArg                FreeArg;
	CellPngDecMainHandle       mHandle = NULL;
	CellPngDecSubHandle        sHandle = NULL;
	CellPngDecThreadInParam    InParam;
	CellPngDecThreadOutParam   OutParam;
	CellPngDecSrc              src;
	CellPngDecOpnInfo          opnInfo;
	CellPngDecInfo             info;
	CellPngDecInParam          inParam;
	CellPngDecOutParam         outParam;
	CellPngDecDataOutInfo      dOutInfo;
	CellPngDecDataCtrlParam    dCtrlParam;

	MallocArg.mallocCallCounts = 0;
	FreeArg.freeCallCounts = 0;
	InParam.spuThreadEnable = CELL_PNGDEC_SPU_THREAD_ENABLE;
	InParam.ppuThreadPriority = 512;
	InParam.spuThreadPriority = 200;
	InParam.cbCtrlMallocFunc = img_malloc;
	InParam.cbCtrlMallocArg = &MallocArg;
	InParam.cbCtrlFreeFunc = img_free;
	InParam.cbCtrlFreeArg = &FreeArg;

	int ret_png, ret = -1;
	ret_png = cellPngDecCreate(&mHandle, &InParam, &OutParam);

	if (ret_png != CELL_OK)
		goto error;

	memset(&src, 0, sizeof(CellPngDecSrc));
	src.srcSelect        = CELL_PNGDEC_FILE;
	src.fileName         = path;
	src.fileOffset       = 0;
	src.fileSize         = 0;
	src.streamPtr        = 0;
	src.streamSize       = 0;

	src.spuThreadEnable  = CELL_PNGDEC_SPU_THREAD_ENABLE;

	ret = cellPngDecOpen(mHandle, &sHandle, &src, &opnInfo);

	if (ret != CELL_OK)
		goto error;

	ret = cellPngDecReadHeader(mHandle, sHandle, &info);

	if (ret != CELL_OK)
		goto error;

	inParam.commandPtr         = NULL;
	inParam.outputMode         = CELL_PNGDEC_TOP_TO_BOTTOM;
	inParam.outputColorSpace   = CELL_PNGDEC_ARGB;
	inParam.outputBitDepth     = 8;
	inParam.outputPackFlag     = CELL_PNGDEC_1BYTE_PER_1PIXEL;
	inParam.outputAlphaSelect  = CELL_PNGDEC_STREAM_ALPHA;
	ret = cellPngDecSetParameter(mHandle, sHandle, &inParam, &outParam);

	if (ret != CELL_OK)
		goto error;

	dCtrlParam.outputBytesPerLine = outParam.outputWidth * 4;
	ret = cellPngDecDecodeData(mHandle, sHandle, data, &dCtrlParam, &dOutInfo);

	if (ret != CELL_OK || dOutInfo.status != CELL_PNGDEC_DEC_STATUS_FINISH)
		goto error;

	width = outParam.outputWidth;
	height = outParam.outputHeight;

	cellPngDecClose(mHandle, sHandle);
	cellPngDecDestroy(mHandle);

	return true;

error:
	if (mHandle && sHandle)
		cellPngDecClose(mHandle, sHandle);
	if (mHandle)
		cellPngDecDestroy(mHandle);
	return false;
}

static void ps3graphics_setup_texture(GLuint tex_tmp, unsigned width, unsigned height)
{
	glBindTexture(GL_TEXTURE_2D, tex_tmp);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, SCREEN_RENDER_PIXEL_FORMAT, width, height, 0, SCREEN_RENDER_PIXEL_FORMAT, GL_UNSIGNED_INT_8_8_8_8, decode_buffer);

	/* Set up texture coord array (TEXCOORD1).*/
	glClientActiveTexture(GL_TEXTURE1);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, (void*)(128 * 3));
	glClientActiveTexture(GL_TEXTURE0);

	/* Go back to old stuff.*/
	glBindTexture(GL_TEXTURE_2D, tex_tmp);
}

bool ps3graphics_load_menu_texture(enum menu_type type, const char * path)
{
	GLuint *texture = NULL;

	switch (type)
	{
		case TEXTURE_BACKDROP:
			texture = &tex_backdrop;
			break;
		case TEXTURE_MENU:
			texture = &tex_menu;
			break;
		default:
			return false;
	}

	unsigned width, height;

	if(strstr(path, ".PNG") != NULL || strstr(path, ".png") != NULL)
	{
		if (!ps3graphics_load_png(path, width, height, decode_buffer))
			return false;
	}
	else
	{
		if (!ps3graphics_load_jpeg(path, width, height, decode_buffer))
			return false;
	}

	if (*texture == 0)
		glGenTextures(1, texture);

	ps3graphics_setup_texture(*texture, width, height);

	return true;
}

/******************************************************************************* 
	Game Aware Shaders
********************************************************************************/

#if 0
static void ps3graphics_load_textures(config_file_t *conf, char *attr)
{
	const char *id = strtok(attr, ";");
	while (id)
	{
		char *path;
		if (!config_get_string(conf, id, &path))
			goto error;

		unsigned width, height;

		lookup_texture tex;
		tex.id = id;

		if(strstr(path, ".PNG") != NULL || strstr(path, ".png") != NULL)
		{
			if (!ps3graphics_load_png(path, width, height, decode_buffer))
				goto error;
		}
		else
		{
			if (!ps3graphics_load_jpeg(path, width, height, decode_buffer))
				goto error;
		}

		glGenTextures(1, &tex.tex);
		ps3graphics_setup_texture(tex.tex, width, height);

		lut_textures.push_back(tex);
		free(path);
		id = strtok(NULL, ";");
	}

	free(attr);
	glBindTexture(GL_TEXTURE_2D, tex);
	return;

error:
	glBindTexture(GL_TEXTURE_2D, tex);
	free(attr);
}
#endif

#if 0
static void ps3graphics_load_imports(config_file_t *conf, char *attr)
{
	std::vector<snes_tracker_uniform_info> info;
	const char *id = strtok(attr, ";");
	while (id)
	{
		snes_tracker_uniform_info uniform;

		char base[MAX_PATH_LENGTH];
		strcpy(base, id);
		char semantic_base[MAX_PATH_LENGTH];
		snprintf(semantic_base, sizeof(semantic_base), "%s_semantic", base);
		char wram_base[MAX_PATH_LENGTH];
		snprintf(wram_base, sizeof(wram_base), "%s_wram", base);
		char input_base[MAX_PATH_LENGTH];
		snprintf(input_base, sizeof(input_base), "%s_input_slot", base);
		char mask_base[MAX_PATH_LENGTH];
		snprintf(mask_base, sizeof(mask_base), "%s_mask", base);

		char *semantic = NULL;
		unsigned addr = 0;

		if (!config_get_string(conf, semantic_base, &semantic))
			goto error;

		int slot = 0;
		enum snes_ram_type ram_type = SSNES_STATE_WRAM;
		if (config_get_int(conf, input_base, &slot))
		{
			switch (slot)
			{
				case 1:
					ram_type = SSNES_STATE_INPUT_SLOT1;
					break;
				case 2:
					ram_type = SSNES_STATE_INPUT_SLOT2;
					break;

				default:
					free(semantic);
					goto error;
			}
		}
		else if (!config_get_hex(conf, wram_base, &addr))
		{
			free(semantic);
			goto error;
		}

		enum snes_tracker_type tracker_type;

		if (addr >= 128 * 1024)
			goto error;

		if (strcmp(semantic, "capture") == 0)
			tracker_type = SSNES_STATE_CAPTURE;
		else if (strcmp(semantic, "transition") == 0)
			tracker_type = SSNES_STATE_TRANSITION;
		else if (strcmp(semantic, "capture_previous") == 0)
			tracker_type = SSNES_STATE_CAPTURE_PREV;
		else if (strcmp(semantic, "transition_previous") == 0)
			tracker_type = SSNES_STATE_TRANSITION_PREV;
		else if (strcmp(semantic, "transition_count") == 0)
			tracker_type = SSNES_STATE_TRANSITION_COUNT;
		else
			goto error;

		unsigned mask = 0;
		char *mask_val;
		if (config_get_string(conf, mask_base, &mask_val))
		{
			mask = strtoul(mask_val, NULL, 16);
			free(mask_val);
		}

		strncpy(uniform.id, id, sizeof(uniform.id));
		uniform.addr = addr;
		uniform.type = tracker_type;
		uniform.mask = mask;
		uniform.ram_type = ram_type;

		info.push_back(uniform);
		free(semantic);

		id = strtok(NULL, ";");
	}

	snes_tracker_info tracker_info;
	tracker_info.wram = (const uint8_t**)&Memory.RAM;
	tracker_info.info = &info[0];
	tracker_info.info_elem = info.size();

	tracker = snes_tracker_init(&tracker_info);

	free(attr);
	return;

error:
	free(attr);
}

void ps3graphics_init_state_uniforms(const char * path)
{
	if (tracker)
	{
		snes_tracker_free(tracker);
		tracker = NULL;

		for (std::vector<lookup_texture>::iterator itr = lut_textures.begin();
				itr != lut_textures.end(); ++itr)
		{
			glDeleteTextures(1, &itr->tex);
		}

		lut_textures.clear();
	}

	config_file_t *conf = config_file_new(path);
	if (!conf)
		return;

	char *textures = NULL;
	char *imports = NULL;
	if (!config_get_string(conf, "textures", &textures))
	{
		return;
	}

	ps3graphics_load_textures(conf, textures);

	if (!config_get_string(conf, "imports", &imports))
	{
		return;
	}

	char * tempshaders[2];

	if (config_get_string(conf, "shader0", &tempshaders[0]))
		ps3graphics_load_fragment_shader(tempshaders[0], 0);

	if (config_get_string(conf, "shader1", &tempshaders[1]))
		ps3graphics_load_fragment_shader(tempshaders[1], 1);

	for(int i = 0; i < 2; i++)
		free(tempshaders[i]);

	ps3graphics_load_imports(conf, imports);

	config_file_free(conf);
}
#endif
