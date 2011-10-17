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

/******************************************************************************* 
	Calculate macros
********************************************************************************/

#define CalculateViewports() \
	float device_aspect = psglGetDeviceAspectRatio(psgl_device); \
   GLuint temp_width = gl_width; \
   GLuint temp_height = gl_height; \
	/* calculate the glOrtho matrix needed to transform the texture to the desired aspect ratio */ \
	/* If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff), assume they are actually equal */ \
   float delta; \
	if (m_ratio == SCREEN_CUSTOM_ASPECT_RATIO) \
	{ \
	  m_viewport_x_temp = m_viewport_x; \
	  m_viewport_y_temp = m_viewport_y; \
	  m_viewport_width_temp = m_viewport_width;  \
	  m_viewport_height_temp = m_viewport_height; \
	} \
	else if ( (int)(device_aspect*1000) > (int)(m_ratio * 1000) ) \
	{ \
     delta = (m_ratio / device_aspect - 1.0) / 2.0 + 0.5; \
	  m_viewport_x_temp = temp_width * (0.5 - delta); \
	  m_viewport_y_temp = 0; \
     m_viewport_width_temp = (int)(2.0 * temp_width * delta); \
	  m_viewport_height_temp = temp_height; \
	} \
	else if ( (int)(device_aspect*1000) < (int)(m_ratio * 1000) ) \
	{ \
     delta = (device_aspect / m_ratio - 1.0) / 2.0 + 0.5; \
	  m_viewport_x_temp = 0; \
	  m_viewport_y_temp = temp_height * (0.5 - delta); \
	  m_viewport_width_temp = temp_width; \
	  m_viewport_height_temp = (int)(2.0 * temp_height * delta); \
	} \
	else \
	{ \
	  m_viewport_x_temp = 0; \
	  m_viewport_y_temp = 0; \
	  m_viewport_width_temp = temp_width; \
	  m_viewport_height_temp = temp_height; \
	} \
	if (m_overscan) \
   { \
      m_left = -m_overscan_amount/2; \
      m_right = 1 + m_overscan_amount/2; \
      m_bottom = -m_overscan_amount/2; \
      m_top = 1 + m_overscan_amount/2; \
      m_zFar = -1; \
      m_zNear = 1; \
   } \
	else \
   { \
      m_left = 0; \
      m_right = 1; \
      m_bottom = 0; \
      m_top = 1; \
      m_zNear = -1; \
      m_zFar = 1; \
   } \
	_cgViewWidth = m_viewport_width_temp; \
	_cgViewHeight = m_viewport_height_temp;

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

int PS3Graphics::calculate_aspect_ratio_before_game_load()
{
   return m_calculate_aspect_ratio_before_game_load;
}

/******************************************************************************* 
	PSGL
********************************************************************************/

PS3Graphics::PS3Graphics(uint32_t resolution, uint32_t aspect, uint32_t smooth, uint32_t smooth2, const char * shader, const char * shader2, const char * menu_shader, uint32_t overscan, float overscan_amount, uint32_t pal60Hz, uint32_t vsync, uint32_t tripleBuffering, uint32_t viewport_x, uint32_t viewport_y, uint32_t viewport_width, uint32_t viewport_height)
{
	//psglgraphics
	psgl_device = NULL;
	psgl_context = NULL;
	gl_width = 0;
	gl_height = 0;

	m_smooth = smooth;
	m_smooth2 = smooth2;
	CheckResolution(resolution) ? m_resolutionId = resolution : m_resolutionId = 0;
	set_aspect_ratio(aspect, 0);

	SetOverscan(overscan, overscan_amount, 0);
	m_pal60Hz = pal60Hz;
	m_vsync = vsync;
	m_tripleBuffering =  tripleBuffering;

	strcpy(_curFragmentShaderPath[0], shader); 
	strcpy(_curFragmentShaderPath[1], shader2); 
	strcpy(_curFragmentShaderPath[2], menu_shader); 

	decode_buffer = (uint8_t*)memalign(128, 2048 * 2048 * 4);
	memset(decode_buffer, 0, (2048 * 2048 * 4));

	init_fbo_memb();

	m_width = 240;
	m_height = 160;

	m_viewport_x = viewport_x;
	m_viewport_y = viewport_y;
	m_viewport_width = viewport_width;
	m_viewport_height = viewport_height;
	m_calculate_aspect_ratio_before_game_load = 1;

	setup_aspect_ratio_array();
}

PS3Graphics::~PS3Graphics()
{
	Deinit();
	free(decode_buffer);
}

void PS3Graphics::Init(uint32_t scaleEnabled, uint32_t scaleFactor)
{
	PSGLInitDevice(m_resolutionId, m_pal60Hz, m_tripleBuffering);
	int32_t ret = PSGLInit(scaleEnabled, scaleFactor);

	if (ret == CELL_OK)
	{
		GetAllAvailableResolutions();
		SetResolution();
	}
}

void PS3Graphics::PSGLInitDevice(uint32_t resolutionId, uint16_t pal60Hz, uint16_t tripleBuffering)
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
		//Resolution setting
		CellVideoOutResolution resolution;
		cellVideoOutGetResolution(resolutionId, &resolution);

		params.enable |= PSGL_DEVICE_PARAMETERS_WIDTH_HEIGHT;
		params.width = resolution.width;
		params.height = resolution.height;
		m_currentResolutionId = resolutionId;
	}

	psgl_device = psglCreateDeviceExtended(&params);

	// Get the dimensions of the screen in question, and do stuff with it :)
	psglGetDeviceDimensions(psgl_device, &gl_width, &gl_height);

	if(m_viewport_width == 0)
		m_viewport_width = gl_width;
	if(m_viewport_height == 0)
		m_viewport_height = gl_height;

	// Create a context and bind it to the current display.
	psgl_context = psglCreateContext();

	psglMakeCurrent(psgl_context, psgl_device);

	psglResetCurrentContext();
}

int32_t PS3Graphics::PSGLInit(uint32_t scaleEnable, uint32_t scaleFactor)
{
	glDisable(GL_DEPTH_TEST);
	set_vsync(m_vsync);

	if(scaleEnable)
		SetFBOScale(scaleEnable, scaleFactor);

	InitCg();

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
	SetSmooth(m_smooth);
	SetSmooth(m_smooth2, 1);

	// PSGL doesn't clear the screen on startup, so let's do that here.
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glColor4f(1.0, 1.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	psglSwap();

	// Vertexes
	GLfloat vertexes[] = {
		0, 0, 0,
		0, 1, 0,
		1, 1, 0,
		1, 0, 0,
		0, 1,
		0, 0,
		1, 0,
		1, 1
	};

	SetScreenDimensions(m_width, m_height, m_width, 0);

	GLfloat vertex_buf[128];
	memcpy(vertex_buf, vertexes, 12 * sizeof(GLfloat));
	memcpy(vertex_buf + 32, vertexes + 12, 8 * sizeof(GLfloat));
	memcpy(vertex_buf + 32 * 3, vertexes + 12, 8 * sizeof(GLfloat));

	glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buf), vertex_buf, GL_STREAM_DRAW);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, 0);
	glTexCoordPointer(2, GL_FLOAT, 0, (void*)128);

	return CELL_OK;
}

void PS3Graphics::init_fbo_memb()
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

	//tracker = NULL;
}

void PS3Graphics::Deinit()
{
	glDeleteTextures(1, &tex);
	PSGLDeInitDevice();
}

void PS3Graphics::PSGLDeInitDevice()
{
	glFinish();
	cellDbgFontExit();

	psglDestroyContext(psgl_context);
	psglDestroyDevice(psgl_device);
#if CELL_SDK_VERSION == 0x340001
	//FIXME: It will crash here for 1.92 - termination of the PSGL library - works fine for 3.41
	psglExit();
#else
	//for 1.92
	gl_width = 0;
	gl_height = 0;
	psgl_context = NULL;
	psgl_device = NULL;
#endif
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

void PS3Graphics::InitDbgFont()
{
	CellDbgFontConfig cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.bufSize = 512;
	cfg.screenWidth = gl_width;
	cfg.screenHeight = gl_height;
	cellDbgFontInit(&cfg);
}

void PS3Graphics::dprintf_noswap(float x, float y, float scale, const char* fmt, ...)
{
	char buffer[512];

	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buffer, 512, fmt, ap);
	cellDbgFontPuts(x, y, scale, 0xffffffff, buffer);
	va_end(ap);

	cellDbgFontDraw();
}

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

void PS3Graphics::Draw(uint8_t *XBuf)
{
	frame_count += 1;
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

void PS3Graphics::DrawMenu(int width, int height)
{
	float device_aspect = psglGetDeviceAspectRatio(psgl_device);
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
	cgGLSetParameter2f(_cgpVideoSize[MENU_SHADER_NO], width, height);
	cgGLSetParameter2f(_cgpTextureSize[MENU_SHADER_NO], width, height);
	cgGLSetParameter2f(_cgpOutputSize[MENU_SHADER_NO], _cgViewWidth, _cgViewHeight);
	cgGLSetParameter2f(_cgp_vertex_VideoSize[MENU_SHADER_NO], width, height);
	cgGLSetParameter2f(_cgp_vertex_TextureSize[MENU_SHADER_NO], width, height);
	cgGLSetParameter2f(_cgp_vertex_OutputSize[MENU_SHADER_NO], _cgViewWidth, _cgViewHeight);

	_cgp_timer[MENU_SHADER_NO] = cgGetNamedParameter(_fragmentProgram[MENU_SHADER_NO], "IN.frame_count");
	_cgp_vertex_timer[MENU_SHADER_NO] = cgGetNamedParameter(_vertexProgram[MENU_SHADER_NO], "IN.frame_count");
	cgGLSetParameter1f(_cgp_timer[MENU_SHADER_NO], frame_count);
	cgGLSetParameter1f(_cgp_vertex_timer[MENU_SHADER_NO], frame_count);
	CGparameter param = cgGetNamedParameter(_fragmentProgram[MENU_SHADER_NO], "bg");
	cgGLSetTextureParameter(param, tex_menu);
	cgGLEnableTextureParameter(param);

	// Set up texture coord array (TEXCOORD1).
	glClientActiveTexture(GL_TEXTURE1);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, (void*)(128 * 3));
	glClientActiveTexture(GL_TEXTURE0);

	glDrawArrays(GL_QUADS, 0, 4); 

	// EnableTextureParameter might overwrite bind in TEXUNIT0.
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	cgGLBindProgram(_vertexProgram[0]);
	cgGLBindProgram(_fragmentProgram[0]);
}

void PS3Graphics::resize_aspect_mode_input_loop(uint64_t state)
{
	if(CTRL_LSTICK_LEFT(state) || CTRL_LEFT(state))
	{
		m_viewport_x -= 1;
	}
	else if (CTRL_LSTICK_RIGHT(state) || CTRL_RIGHT(state))
	{
		m_viewport_x += 1;
	}		
	if (CTRL_LSTICK_UP(state) || CTRL_UP(state))
	{
		m_viewport_y += 1;
	}
	else if (CTRL_LSTICK_DOWN(state) || CTRL_DOWN(state)) 
	{
		m_viewport_y -= 1;
	}

	if (CTRL_RSTICK_LEFT(state) || CTRL_L1(state))
	{
		m_viewport_width -= 1;
	}
	else if (CTRL_RSTICK_RIGHT(state) || CTRL_R1(state))
	{
		m_viewport_width += 1;
	}		
	if (CTRL_RSTICK_UP(state) || CTRL_L2(state))
	{
		m_viewport_height += 1;
	}
	else if (CTRL_RSTICK_DOWN(state) || CTRL_R2(state))
	{
		m_viewport_height -= 1;
	}

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

void PS3Graphics::ChangeResolution(uint32_t resId, uint16_t pal60Hz, uint16_t tripleBuffering, uint32_t scaleEnabled, uint32_t scaleFactor)
{
	cellDbgFontExit();
	PSGLDeInitDevice();

	PSGLInitDevice(resId, pal60Hz, tripleBuffering);
	PSGLInit(scaleEnabled, scaleFactor);
	InitDbgFont();
	SetResolution();
}

int PS3Graphics::CheckResolution(uint32_t resId)
{
	return cellVideoOutGetResolutionAvailability(CELL_VIDEO_OUT_PRIMARY, resId, CELL_VIDEO_OUT_ASPECT_AUTO,0);
}

void PS3Graphics::NextResolution()
{
	if(m_currentResolutionPos+1 < m_supportedResolutions.size())
	{
		m_currentResolutionPos++;
		m_currentResolutionId = m_supportedResolutions[m_currentResolutionPos];
	}
}

void PS3Graphics::PreviousResolution()
{
	if(m_currentResolutionPos > 0)
	{
		m_currentResolutionPos--;
		m_currentResolutionId = m_supportedResolutions[m_currentResolutionPos];
	}
}

void PS3Graphics::SwitchResolution(uint32_t resId, uint16_t pal60Hz, uint16_t tripleBuffering, uint32_t scaleEnabled, uint32_t scaleFactor)
{
	if(CheckResolution(resId))
	{
		ChangeResolution(resId, pal60Hz, tripleBuffering, scaleEnabled, scaleFactor);
		SetFBOScale(scaleEnabled, scaleFactor);
	}
	LoadFragmentShader(DEFAULT_MENU_SHADER_FILE, 2);
}

/******************************************************************************* 
	Cg
********************************************************************************/

int32_t PS3Graphics::InitCg()
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
		if (strlen(_curFragmentShaderPath[i]) > 0)
		{
			if( LoadFragmentShader(_curFragmentShaderPath[i], i) != CELL_OK)
				return 1;
		}
		else
		{
			strcpy(_curFragmentShaderPath[i], DEFAULT_SHADER_FILE);
			if (LoadFragmentShader(_curFragmentShaderPath[i], i) != CELL_OK)
				return 1;
		}
	}

	return CELL_OK;
}

CGprogram LoadShaderFromSource(CGcontext cgtx, CGprofile target, const char* filename, const char* entry)
{
	const char* args[] = { "-fastmath", "-unroll=all", "-ifcvt=all", 0 };
	CGprogram id = cgCreateProgramFromFile(cgtx, CG_SOURCE, filename, target, entry, args);

	return id;
}

int32_t PS3Graphics::LoadFragmentShader(const char * shaderPath, unsigned index)
{
	// store the current path
	strcpy(_curFragmentShaderPath[index], shaderPath);

	_vertexProgram[index] = LoadShaderFromSource(_cgContext, CG_PROFILE_SCE_VP_RSX, shaderPath, "main_vertex");
	_fragmentProgram[index] = LoadShaderFromSource(_cgContext, CG_PROFILE_SCE_FP_RSX, shaderPath, "main_fragment");

	// bind and enable the vertex and fragment programs
	cgGLEnableProfile(CG_PROFILE_SCE_VP_RSX);
	cgGLEnableProfile(CG_PROFILE_SCE_FP_RSX);
	cgGLBindProgram(_vertexProgram[index]);
	cgGLBindProgram(_fragmentProgram[index]);

	// acquire mvp param from v shader
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

void PS3Graphics::ResetFrameCounter()
{
	frame_count = 0;
}

/******************************************************************************* 
	Get functions
********************************************************************************/

uint32_t PS3Graphics::GetInitialResolution()
{
	return m_initialResolution;
}

uint32_t PS3Graphics::GetPAL60Hz()
{
	return m_pal60Hz;
}

void PS3Graphics::GetAllAvailableResolutions()
{
	bool defaultresolution = true;
	uint32_t videomode[] = {
		CELL_VIDEO_OUT_RESOLUTION_480, CELL_VIDEO_OUT_RESOLUTION_576,
		CELL_VIDEO_OUT_RESOLUTION_960x1080, CELL_VIDEO_OUT_RESOLUTION_720,
		CELL_VIDEO_OUT_RESOLUTION_1280x1080, CELL_VIDEO_OUT_RESOLUTION_1440x1080,
		CELL_VIDEO_OUT_RESOLUTION_1600x1080, CELL_VIDEO_OUT_RESOLUTION_1080};

	// Provide future expandability of the videomode array
	uint16_t num_videomodes = sizeof(videomode)/sizeof(uint32_t);
	for (int i=0; i < num_videomodes; i++) {
		if (CheckResolution(videomode[i]))
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

	// In case we didn't specify a resolution - make the last resolution
	// that was added to the list (the highest resolution) the default resolution
	if (m_currentResolutionPos > num_videomodes | defaultresolution)
		m_currentResolutionPos = m_supportedResolutions.size()-1;
}

uint32_t PS3Graphics::GetCurrentResolution()
{
	return m_supportedResolutions[m_currentResolutionPos];
}

uint32_t PS3Graphics::get_viewport_x(void)
{
	return m_viewport_x;
}

uint32_t PS3Graphics::get_viewport_y(void)
{
	return m_viewport_y;
}

uint32_t PS3Graphics::get_viewport_width(void)
{
	return m_viewport_width;
}

uint32_t PS3Graphics::get_viewport_height(void)
{
	return m_viewport_height;
}

int PS3Graphics::get_aspect_ratio_int(bool orientation)
{
	//orientation true is aspect_y, false is aspect_x
	if(orientation)
		return aspect_y;
	else
		return aspect_x;
}

float PS3Graphics::get_aspect_ratio_float(int enum_)
{
	return aspectratios[enum_];
}

const char * PS3Graphics::GetResolutionLabel(uint32_t resolution)
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

CellVideoOutState PS3Graphics::GetVideoOutState()
{
	return m_stored_video_state;
}

/******************************************************************************* 
	Set functions
********************************************************************************/

void PS3Graphics::SetFBOScale(uint32_t enable, unsigned scale)
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

void PS3Graphics::SetResolution()
{
	cellVideoOutGetState(CELL_VIDEO_OUT_PRIMARY, 0, &m_stored_video_state);
}

void PS3Graphics::SetOverscan(uint32_t will_overscan, float amount, uint32_t setviewport)
{
	m_overscan_amount = amount;
	m_overscan = will_overscan;


	CalculateViewports();
	if(setviewport)
	{
		SetViewports();
	}
}

void PS3Graphics::SetPAL60Hz(uint32_t pal60Hz)
{
	m_pal60Hz = pal60Hz;
}

void PS3Graphics::SetTripleBuffering(uint32_t triple_buffering)
{
	m_tripleBuffering = triple_buffering;
}

void PS3Graphics::SetSmooth(uint32_t smooth, unsigned index)
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
		m_smooth2 = smooth;
		glBindTexture(GL_TEXTURE_2D, fbo_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_smooth2 ? GL_LINEAR : GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_smooth2 ? GL_LINEAR : GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, tex);
	}
}

void PS3Graphics::set_aspect_ratio(uint32_t aspect, uint32_t width, uint32_t height, uint32_t setviewport)
{
	switch(aspect)
	{
		case ASPECT_RATIO_1_1:
			m_ratio = SCREEN_1_1_ASPECT_RATIO;
			aspect_x = 1;
			aspect_y = 1;
			m_calculate_aspect_ratio_before_game_load = 0;
			break;
		case ASPECT_RATIO_4_3:
			m_ratio = SCREEN_4_3_ASPECT_RATIO;
			aspect_x = 4;
			aspect_y = 3;
			m_calculate_aspect_ratio_before_game_load = 0;
			break;
		case ASPECT_RATIO_5_4:
			m_ratio = SCREEN_5_4_ASPECT_RATIO;
			aspect_x = 5;
			aspect_y = 4;
			m_calculate_aspect_ratio_before_game_load = 0;
			break;
		case ASPECT_RATIO_8_7:
			m_ratio = SCREEN_8_7_ASPECT_RATIO;
			aspect_x = 8;
			aspect_y = 7;
			m_calculate_aspect_ratio_before_game_load = 0;
			break;
		case ASPECT_RATIO_16_9:
			m_ratio = SCREEN_16_9_ASPECT_RATIO;
			aspect_x = 16;
			aspect_y = 9;
			m_calculate_aspect_ratio_before_game_load = 0;
			break;
		case ASPECT_RATIO_16_10:
			m_ratio = SCREEN_16_10_ASPECT_RATIO;
			aspect_x = 16;
			aspect_y = 10;
			m_calculate_aspect_ratio_before_game_load = 0;
			break;
		case ASPECT_RATIO_16_15:
			m_ratio = SCREEN_16_15_ASPECT_RATIO;
			aspect_x = 16;
			aspect_y = 15;
			m_calculate_aspect_ratio_before_game_load = 0;
			break;
		case ASPECT_RATIO_19_14:
			m_ratio = SCREEN_19_14_ASPECT_RATIO;
			aspect_x = 19;
			aspect_y = 14;
			m_calculate_aspect_ratio_before_game_load = 0;
			break;
		case ASPECT_RATIO_2_1:
			m_ratio = SCREEN_2_1_ASPECT_RATIO;
			aspect_x = 2;
			aspect_y = 1;
			m_calculate_aspect_ratio_before_game_load = 0;
			break;
		case ASPECT_RATIO_3_2:
			m_ratio = SCREEN_3_2_ASPECT_RATIO;
			aspect_x = 3;
			aspect_y = 2;
			m_calculate_aspect_ratio_before_game_load = 0;
			break;
		case ASPECT_RATIO_3_4:
			m_ratio = SCREEN_3_4_ASPECT_RATIO;
			aspect_x = 3;
			aspect_y = 4;
			m_calculate_aspect_ratio_before_game_load = 0;
			break;
		case ASPECT_RATIO_AUTO:
			m_calculate_aspect_ratio_before_game_load = 1;
			if(width != 0 && height != 0)
			{
				unsigned len = width < height ? width : height;
				unsigned highest = 1;
				for (unsigned i = 1; i < len; i++)
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
			m_calculate_aspect_ratio_before_game_load = 0;
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

void PS3Graphics::SetDimensions(unsigned width, unsigned height, unsigned pitch)
{
	SetScreenDimensions(width, height, pitch, 1);
	fbo_vp_width = m_width * fbo_scale;
	fbo_vp_height = m_height * fbo_scale;
}

void PS3Graphics::set_vsync(uint32_t vsync)
{
	vsync ? glEnable(GL_VSYNC_SCE) : glDisable(GL_VSYNC_SCE);
}

uint32_t PS3Graphics::SetTextMessageSpeed(uint32_t value)
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

bool PS3Graphics::load_jpeg(const std::string& path, unsigned &width, unsigned &height, uint8_t *data)
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

	int ret_jpeg, ret_file, ret, ok = -1;
	ret_jpeg = cellJpgDecCreate(&mHandle, &InParam, &OutParam);

	if (ret_jpeg != CELL_OK)
	{
		goto error;
	}

	memset(&src, 0, sizeof(CellJpgDecSrc));
	src.srcSelect        = CELL_JPGDEC_FILE;
	src.fileName         = path.c_str();
	src.fileOffset       = 0;
	src.fileSize         = 0;
	src.streamPtr        = NULL;
	src.streamSize       = 0;

	src.spuThreadEnable  = CELL_JPGDEC_SPU_THREAD_ENABLE;

	ret_file = ret = cellJpgDecOpen(mHandle, &sHandle, &src, &opnInfo);

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

bool PS3Graphics::load_png(const std::string& path, unsigned &width, unsigned &height, uint8_t *data)
{
	// Holy shit, Sony!
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

	int ret_png, ret_file, ret, ok = -1;
	ret_png = cellPngDecCreate(&mHandle, &InParam, &OutParam);

	if (ret_png != CELL_OK)
		goto error;

	memset(&src, 0, sizeof(CellPngDecSrc));
	src.srcSelect        = CELL_PNGDEC_FILE;
	src.fileName         = path.c_str();
	src.fileOffset       = 0;
	src.fileSize         = 0;
	src.streamPtr        = 0;
	src.streamSize       = 0;

	src.spuThreadEnable  = CELL_PNGDEC_SPU_THREAD_ENABLE;

	ret_file = ret = cellPngDecOpen(mHandle, &sHandle, &src, &opnInfo);

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

	// Is this pitch?
	dCtrlParam.outputBytesPerLine = 1920 * 4;
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

void PS3Graphics::setup_texture(GLuint tex, unsigned width, unsigned height)
{
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, SCREEN_RENDER_PIXEL_FORMAT, width, height, 0, SCREEN_RENDER_PIXEL_FORMAT, GL_UNSIGNED_INT_8_8_8_8, decode_buffer);

	// Set up texture coord array (TEXCOORD1).
	glClientActiveTexture(GL_TEXTURE1);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, (void*)(128 * 3));
	glClientActiveTexture(GL_TEXTURE0);

	// Go back to old stuff.
	glBindTexture(GL_TEXTURE_2D, tex);
}

bool PS3Graphics::LoadMenuTexture(enum menu_type type, const std::string &path)
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

	if(strstr(path.c_str(), ".PNG") != NULL || strstr(path.c_str(), ".png") != NULL)
	{
		if (!load_png(path, width, height, decode_buffer))
			return false;
	}
	else
	{
		if (!load_jpeg(path, width, height, decode_buffer))
			return false;
	}

	if (*texture == 0)
		glGenTextures(1, texture);

	setup_texture(*texture, width, height);

	return true;
}
