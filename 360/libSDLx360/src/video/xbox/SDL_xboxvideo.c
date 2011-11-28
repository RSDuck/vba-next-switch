/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002  Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@libsdl.org
*/

#ifdef SAVE_RCSID
static char rcsid =
 "@(#) $Id: SDL_xboxvideo.c,v 1.1 2003/07/18 15:19:33 lantus Exp $";
#endif

/* XBOX SDL video driver implementation; this is just enough to make an
 *  SDL-based application THINK it's got a working video driver, for
 *  applications that call SDL_Init(SDL_INIT_VIDEO) when they don't need it,
 *  and also for use as a collection of stubs when porting SDL to a new
 *  platform for which you haven't yet written a valid video driver.
 *
 * This is also a great way to determine bottlenecks: if you think that SDL
 *  is a performance problem for a given platform, enable this driver, and
 *  then see if your application runs faster without video overhead.
 *
 * Initial work by Ryan C. Gordon (icculus@linuxgames.com). A good portion
 *  of this was cut-and-pasted from Stephane Peter's work in the AAlib
 *  SDL video driver.  Renamed to "XBOX" by Sam Lantinga.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "SDL.h"
#include "SDL_error.h"
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "SDL_sysvideo.h"
#include "SDL_pixels_c.h"
#include "SDL_events_c.h"

#include "SDL_xboxvideo.h"
#include "SDL_xboxevents_c.h"
#include "SDL_xboxmouse_c.h"
#include "../SDL_yuvfuncs.h"

#define XBOXVID_DRIVER_NAME "XBOX"
 

/* Initialization/Query functions */
static int XBOX_VideoInit(_THIS, SDL_PixelFormat *vformat);
static SDL_Rect **XBOX_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
static SDL_Surface *XBOX_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
static int XBOX_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors);
static void XBOX_VideoQuit(_THIS);

/* Hardware surface functions */
static int XBOX_AllocHWSurface(_THIS, SDL_Surface *surface);
static int XBOX_LockHWSurface(_THIS, SDL_Surface *surface);
static void XBOX_UnlockHWSurface(_THIS, SDL_Surface *surface);
static void XBOX_FreeHWSurface(_THIS, SDL_Surface *surface);
static int XBOX_RenderSurface(_THIS, SDL_Surface *surface);
static int XBOX_FillHWRect(_THIS, SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color);
static int XBOX_CheckHWBlit(_THIS, SDL_Surface *src, SDL_Surface *dst);
static int XBOX_HWAccelBlit(SDL_Surface *src, SDL_Rect *srcrect,SDL_Surface *dst, SDL_Rect *dstrect);
static int XBOX_SetHWAlpha(_THIS, SDL_Surface *surface, Uint8 alpha);
static int XBOX_SetHWColorKey(_THIS, SDL_Surface *surface, Uint32 key);
static int XBOX_SetFlickerFilter(_THIS, SDL_Surface *surface, int filter);
static int XBOX_SetSoftDisplayFilter(_THIS, SDL_Surface *surface, int enabled);


/* The functions used to manipulate software video overlays */
static struct private_yuvhwfuncs XBOX_yuvfuncs = {
	XBOX_LockYUVOverlay,
	XBOX_UnlockYUVOverlay,
	XBOX_DisplayYUVOverlay,
	XBOX_FreeYUVOverlay
};

struct private_yuvhwdata {
	LPDIRECT3DTEXTURE9 surface;
	
	/* These are just so we don't have to allocate them separately */
	Uint16 pitches[3];
	Uint8 *planes[3];
};

 

int have_vertexbuffer=0;
int have_direct3dtexture=0;

D3DVERTEXELEMENT9 decl[] = 
{
{ 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
{ 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
D3DDECL_END()
};

//--------------------------------------------------------------------------------------
// Vertex and pixel shaders for gradient background rendering
//--------------------------------------------------------------------------------------
static const CHAR* g_strGradientShader =
    "struct VS_IN                                              \n"
    "{                                                         \n"
    "   float4   Position     : POSITION;                      \n"
    "   float2   UV        : TEXCOORD0;                        \n"
    "};                                                        \n"
    "                                                          \n"
    "struct VS_OUT                                             \n"
    "{                                                         \n"
    "   float4 Position       : POSITION;                      \n"
    "   float2 UV        : TEXCOORD0;                        \n"
    "};                                                        \n"
    "                                                          \n"
    "VS_OUT GradientVertexShader( VS_IN In )                   \n"
    "{                                                         \n"
    "   VS_OUT Out;                                            \n"
    "   Out.Position = In.Position;                            \n"
    "   Out.UV  = In.UV;                               \n"
    "   return Out;                                            \n"
    "}                                                         \n";
   
//-------------------------------------------------------------------------------------
// Pixel shader
//-------------------------------------------------------------------------------------
const char*                                 g_strPixelShaderProgram =
    " struct PS_IN                                 "
    " {                                            "
    "     float2 TexCoord : TEXCOORD;              "
    " };                                           "  // the vertex shader
    "                                              "
    " sampler detail;                              "
    "                                              "
    " float4 main( PS_IN In ) : COLOR              "
    " {                                            "
    "     return tex2D( detail, In.TexCoord );     "  // Output color
    " }     ";   
 
 
static void XBOX_UpdateRects(_THIS, int numrects, SDL_Rect *rects);

/* XBOX driver bootstrap functions */

static int XBOX_Available(void)
{
	return(1);
}

static void XBOX_DeleteDevice(SDL_VideoDevice *device)
{
	free(device->hidden);
	free(device);
}

static SDL_VideoDevice *XBOX_CreateDevice(int devindex)
{
	SDL_VideoDevice *device;

	/* Initialize all variables that we clean on shutdown */
	device = (SDL_VideoDevice *)malloc(sizeof(SDL_VideoDevice));
	if ( device ) {
		memset(device, 0, (sizeof *device));
		device->hidden = (struct SDL_PrivateVideoData *)
				malloc((sizeof *device->hidden));
	}
	if ( (device == NULL) || (device->hidden == NULL) ) {
		SDL_OutOfMemory();
		if ( device ) {
			free(device);
		}
		return(0);
	}
	memset(device->hidden, 0, (sizeof *device->hidden));

	/* Set the function pointers */
	device->VideoInit = XBOX_VideoInit;
	device->ListModes = XBOX_ListModes;
	device->SetVideoMode = XBOX_SetVideoMode;
	device->CreateYUVOverlay = NULL;
	device->SetColors = XBOX_SetColors;
	device->UpdateRects = XBOX_UpdateRects;
	device->VideoQuit = XBOX_VideoQuit;
	device->AllocHWSurface = XBOX_AllocHWSurface;
	device->CheckHWBlit = NULL;
	device->FillHWRect = NULL;
	device->SetHWColorKey = XBOX_SetHWColorKey;
	device->SetHWAlpha = NULL;
	device->LockHWSurface = XBOX_LockHWSurface;
	device->UnlockHWSurface = XBOX_UnlockHWSurface;
	device->FlipHWSurface = XBOX_RenderSurface;
	device->FreeHWSurface = XBOX_FreeHWSurface;
	device->SetCaption = NULL;
	device->SetIcon = NULL;
	device->IconifyWindow = NULL;
	device->GrabInput = NULL;
	device->GetWMInfo = NULL;
	device->InitOSKeymap = XBOX_InitOSKeymap;
	device->PumpEvents = XBOX_PumpEvents;
	device->free = XBOX_DeleteDevice;
	device->SetGammaRamp = XBOX_SetGammaRamp;

	return device;
}

VideoBootStrap XBOX_bootstrap = {
	XBOXVID_DRIVER_NAME, "XBOX 360 SDL video driver V0.01",
	XBOX_Available, XBOX_CreateDevice
};


int XBOX_VideoInit(_THIS, SDL_PixelFormat *vformat)
{	 
	if (!D3D)
		D3D = Direct3DCreate9(D3D_SDK_VERSION);

	ZeroMemory(&D3D_PP,sizeof(D3D_PP));



	D3D_PP.BackBufferWidth = 1280;
	D3D_PP.BackBufferHeight = 720;
	D3D_PP.BackBufferFormat = D3DFMT_A8R8G8B8;
	D3D_PP.PresentationInterval = D3DPRESENT_INTERVAL_ONE ;
 
	if (!D3D_Device)
		IDirect3D9_CreateDevice(D3D,0,D3DDEVTYPE_HAL,NULL,D3DCREATE_NO_SHADER_PATCHING ,&D3D_PP,&D3D_Device);
	else
		D3DDevice_Reset(D3D_Device, &D3D_PP);


	vformat->BitsPerPixel = 32;
	vformat->BytesPerPixel = 4;

	vformat->Amask = 0xFF000000;
	vformat->Rmask = 0x00FF0000;
	vformat->Gmask = 0x0000FF00;
	vformat->Bmask = 0x000000FF;


	if (D3D_Device)
		return(1);
	else
		return (0);
}

const static SDL_Rect
	RECT_1280x720 = {0,0,1280,720},
	RECT_1024x768 = {0,0,1024,768},
	RECT_800x600 = {0,0,800,600},
	RECT_640x480 = {0,0,640,480},
	RECT_640x400 = {0,0,640,400},
	RECT_512x384 = {0,0,512,384},
    RECT_480x320 = {0,0,480,320},
	RECT_400x300 = {0,0,400,300},	
	RECT_320x240 = {0,0,320,240},
	RECT_320x200 = {0,0,320,200},
	RECT_240x160 = {0,0,240,160};
const static SDL_Rect *vid_modes[] = {
	&RECT_1280x720,
	&RECT_1024x768,
	&RECT_800x600,
	&RECT_640x480,
	&RECT_640x400,
	&RECT_512x384,
    &RECT_480x320,
	&RECT_400x300,
	&RECT_320x240,
	&RECT_320x200,
	&RECT_240x160,
	NULL
};


SDL_Rect **XBOX_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags)
{
	return &vid_modes;
}

SDL_Surface *XBOX_SetVideoMode(_THIS, SDL_Surface *current,
				int width, int height, int bpp, Uint32 flags)
{

	RECT drawRect;
	float screenAspect;
	void *pLockedVertexBuffer;
	unsigned int afterRenderWidth;
	unsigned int afterRenderHeight;
	float renderWidthCalc;
	float renderHeightCalc;
	float tX = 1;
	float tY = 1;
	float xFactor;
	float yFactor;
	float minFactor;
  
	VERTEX a;
	VERTEX b;
	VERTEX c;
	VERTEX d;

	int pixel_mode,pitch;
	Uint32 Rmask, Gmask, Bmask;

	HRESULT ret;

	switch(bpp)
	{
		case 15:
			pitch = width*2;
			Rmask = 0x00007c00;
			Gmask = 0x000003e0;
			Bmask = 0x0000001f;
			pixel_mode = D3DFMT_LIN_X1R5G5B5;
			break;
		case 8:
		case 16:
			pitch = width*2;
			Rmask = 0x0000f800;
			Gmask = 0x000007e0;
			Bmask = 0x0000001f;
			pixel_mode = D3DFMT_LIN_R5G6B5;
			break;
		case 24:
		case 32:
			pitch = width*4;
			pixel_mode = D3DFMT_LIN_X8R8G8B8;
			Rmask = 0x00FF0000;
			Gmask = 0x0000FF00;
			Bmask = 0x000000FF;
			break;
		default:
			SDL_SetError("Couldn't find requested mode in list");
			return(NULL);
	}

	/* Allocate the new pixel format for the screen */
	if ( ! SDL_ReallocFormat(current, bpp, Rmask, Gmask, Bmask, 0) ) {
		SDL_SetError("Couldn't allocate new pixel format for requested mode");
		return(NULL);
	}

 
	ret = IDirect3DDevice9_CreateTexture(D3D_Device,width,height,1, 0,pixel_mode, D3DUSAGE_CPU_CACHED_MEMORY, (D3DTexture**)&this->hidden->SDL_primary, NULL);
	have_direct3dtexture=1;

	if (ret != D3D_OK)
	{
		SDL_SetError("Couldn't create Direct3D Texture!");
		return(NULL);
	}

 
   // Compile pixel shader.

   D3DXCompileShader( g_strPixelShaderProgram,
                            ( UINT )strlen( g_strPixelShaderProgram ),
                            NULL,
                            NULL,
                            "main",
                            "ps_2_0",
                            0,
                            &pPixelShaderCode,
                            &pPixelErrorMsg,
                            NULL );
    
    // Create pixel shader.
	g_pPixelShader = D3DDevice_CreatePixelShader( ( DWORD* )pPixelShaderCode->lpVtbl->GetBufferPointer(pPixelShaderCode));
	 
	    // Create vertex declaration
    if( NULL == g_pGradientVertexDecl )
    {
        static const D3DVERTEXELEMENT9 decl[] =
        {
			{ 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			{ 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };

       
		 IDirect3DDevice9_CreateVertexDeclaration(D3D_Device,decl,&g_pGradientVertexDecl);
            
    }
 
	    // Create vertex shader
    if( NULL == g_pGradientVertexShader )
    {
        ID3DXBuffer* pShaderCode;
        if( FAILED( D3DXCompileShader( g_strGradientShader, strlen( g_strGradientShader ),
                                       NULL, NULL, "GradientVertexShader", "vs_2_0", 0,
                                       &pShaderCode, NULL, NULL ) ) )
            return 0;
		 
        IDirect3DDevice9_CreateVertexShader(D3D_Device,( DWORD* )pShaderCode->lpVtbl->GetBufferPointer(pShaderCode), &g_pGradientVertexShader);
    }
 	 
	IDirect3DDevice9_CreateVertexDeclaration(D3D_Device,decl,&vertexDeclaration);
	IDirect3DDevice9_CreateVertexBuffer(D3D_Device,sizeof(triangleStripVertices), D3DUSAGE_CPU_CACHED_MEMORY,0L, D3DPOOL_DEFAULT, &vertexBuffer, NULL );

	D3DDevice_SetVertexShader(D3D_Device, g_pGradientVertexShader );
	D3DDevice_SetPixelShader(D3D_Device, g_pPixelShader);
	D3DDevice_SetVertexDeclaration(D3D_Device, g_pGradientVertexDecl);
	D3DDevice_SetRenderState(D3D_Device, D3DRS_VIEWPORTENABLE, FALSE);

	D3DDevice_SetSamplerState(D3D_Device, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	D3DDevice_SetSamplerState(D3D_Device, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
 
  
	IDirect3DVertexBuffer9_Lock(vertexBuffer, 0, 0, (BYTE **)&pLockedVertexBuffer, 0L );	 

	if ( flags & SDL_RESIZABLE)
	{
		screenAspect = (float)(float)width /height;

		afterRenderWidth = width << 1; 
		afterRenderHeight = height << 1;

		renderWidthCalc = (float)afterRenderWidth;
		renderHeightCalc = (float)afterRenderHeight;
		if(renderWidthCalc/renderHeightCalc>screenAspect)
			renderWidthCalc = renderHeightCalc * screenAspect;
		else if(renderWidthCalc/renderHeightCalc<screenAspect)
			renderHeightCalc = renderWidthCalc / screenAspect;

		xFactor = (float)D3D_PP.BackBufferWidth / renderWidthCalc;
		yFactor = (float)D3D_PP.BackBufferHeight / renderHeightCalc;
		minFactor = xFactor < yFactor ? xFactor : yFactor;


		drawRect.right = (LONG)(renderWidthCalc * minFactor);
		drawRect.bottom = (LONG)(renderHeightCalc * minFactor);

		drawRect.left = (D3D_PP.BackBufferWidth - drawRect.right) / 2;
		drawRect.top = (D3D_PP.BackBufferHeight - drawRect.bottom) / 2;

		drawRect.right += drawRect.left;
		drawRect.bottom += drawRect.top;
	}
	else
	{
		drawRect.top = 40;
		drawRect.left = 80;
		drawRect.right = D3D_PP.BackBufferWidth - 80;
		drawRect.bottom = D3D_PP.BackBufferHeight- 40;
	}

	a.x = (float)drawRect.left - 0.5f;
	a.y = (float)drawRect.bottom - 0.5f;
	a.z = 0;
	a.rhw = 1;
	a.tx = 0;
	a.ty = tY;

	b.x = (float)drawRect.left - 0.5f;
	b.y = (float)drawRect.top - 0.5f;
	b.z = 0;
	b.rhw = 1;
	b.tx = 0;
	b.ty = 0;

	c.x = (float)drawRect.right - 0.5f;
	c.y = (float)drawRect.bottom - 0.5f;
	c.z = 0;
	c.rhw = 1;
	c.tx = tX;
	c.ty = tY;

	d.x = (float)drawRect.right - 0.5f;
	d.y = (float)drawRect.top - 0.5f;
	d.z = 0;
	d.rhw = 1;
	d.tx = tX;
	d.ty = 0;


	triangleStripVertices[0] = a;
	triangleStripVertices[1] = b;
	triangleStripVertices[2] = c;
	triangleStripVertices[3] = d;
	
	memcpy(pLockedVertexBuffer,triangleStripVertices,sizeof(triangleStripVertices));
	IDirect3DVertexBuffer9_Unlock(vertexBuffer);
	IDirect3DDevice9_Clear(D3D_Device, 0, NULL, D3DCLEAR_TARGET , 0x00000000, 1.0f, 0L);
	IDirect3DDevice9_SetTexture(D3D_Device, 0, (D3DBaseTexture *)this->hidden->SDL_primary);
	IDirect3DDevice9_SetStreamSource(D3D_Device, 0,vertexBuffer,0,sizeof(VERTEX));		
    	 
	have_vertexbuffer=1;


	/* Set up the new mode framebuffer */
	current->flags = (SDL_FULLSCREEN|SDL_HWSURFACE);

	if (flags & SDL_DOUBLEBUF)
		current->flags |= SDL_DOUBLEBUF;

	current->w = width;
	current->h = height;
	current->pitch = current->w * (bpp / 8);
	current->pixels = NULL;


	IDirect3DDevice9_Clear(D3D_Device, 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L);
	IDirect3DDevice9_Present(D3D_Device,NULL,NULL,NULL,NULL);
 
	/* We're done */
	return(current);
}

/* We don't actually allow hardware surfaces other than the main one */

static int XBOX_AllocHWSurface(_THIS, SDL_Surface *surface)
{

	return(-1);
}
static void XBOX_FreeHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}

static int XBOX_RenderSurface(_THIS, SDL_Surface *surface)
{
	static int    i = 0;
	
	D3DDevice_SetVertexShader(D3D_Device, NULL );
	D3DDevice_SetPixelShader(D3D_Device, NULL);
	D3DDevice_SetVertexDeclaration(D3D_Device, NULL);
 
    // Render the image

	D3DDevice_SetVertexShader(D3D_Device, g_pGradientVertexShader );
	D3DDevice_SetPixelShader(D3D_Device, g_pPixelShader);
	D3DDevice_SetVertexDeclaration(D3D_Device, g_pGradientVertexDecl);

    IDirect3DDevice9_DrawPrimitive(D3D_Device,  D3DPT_TRIANGLESTRIP, 0, 2 ); 
	IDirect3DDevice9_Present(D3D_Device,NULL,NULL,NULL,NULL);

	return (1);
}

static int XBOX_FillHWRect(_THIS, SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color)
{
	HRESULT ret;
	ret = IDirect3DDevice9_Clear(D3D_Device, 0, NULL, D3DCLEAR_TARGET, color, 1.0f, 0L);

	if (ret == D3D_OK)
		return (1);
	else
		return (0);
}


static int XBOX_HWAccelBlit(SDL_Surface *src, SDL_Rect *srcrect,
					SDL_Surface *dst, SDL_Rect *dstrect)
{
	return(1);
}

static int XBOX_CheckHWBlit(_THIS, SDL_Surface *src, SDL_Surface *dst)
{
	return(0);
}

/* We need to wait for vertical retrace on page flipped displays */
static int XBOX_LockHWSurface(_THIS, SDL_Surface *surface)
{
	HRESULT ret;
	D3DLOCKED_RECT d3dlr;

	if (!this->hidden->SDL_primary)
		return (-1);

	ret = IDirect3DTexture9_LockRect(this->hidden->SDL_primary, 0, &d3dlr, NULL, 0);

	surface->pitch = d3dlr.Pitch;
	surface->pixels = d3dlr.pBits;

	if (ret == D3D_OK)
		return(0);
	else
		return(-1);
}

static void XBOX_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
	IDirect3DTexture9_UnlockRect(this->hidden->SDL_primary,0);

	return;
}

static void XBOX_UpdateRects(_THIS, int numrects, SDL_Rect *rects)
{
 
	if (!this->hidden->SDL_primary)
		return;

    // Render the image
	D3DDevice_SetVertexShader(D3D_Device, NULL );
	D3DDevice_SetPixelShader(D3D_Device, NULL);
	D3DDevice_SetVertexDeclaration(D3D_Device, NULL);
 
    // Render the image

	D3DDevice_SetVertexShader(D3D_Device, g_pGradientVertexShader );
	D3DDevice_SetPixelShader(D3D_Device, g_pPixelShader);
	D3DDevice_SetVertexDeclaration(D3D_Device, g_pGradientVertexDecl);

	IDirect3DDevice9_DrawPrimitive(D3D_Device,  D3DPT_TRIANGLESTRIP, 0, 2 ); 
	IDirect3DDevice9_Present(D3D_Device,NULL,NULL,NULL,NULL);


}

int XBOX_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
	return(1);
}

/* Note:  If we are terminated, this could be called in the middle of
   another SDL video routine -- notably UpdateRects.
*/
void XBOX_VideoQuit(_THIS)
{
	 if (this->hidden->SDL_primary)
	 {
        IDirect3DDevice9_SetTexture(D3D_Device, 0, NULL);
		IDirect3DDevice9_SetStreamSource(D3D_Device, 0,NULL, 0, 0);	
		IDirect3DTexture9_Release(this->hidden->SDL_primary);

		D3DDevice_SetVertexShader(D3D_Device, NULL );
		D3DDevice_SetPixelShader(D3D_Device, NULL);
		D3DDevice_SetVertexDeclaration(D3D_Device, NULL);
	 }

	 if (g_pGradientVertexDecl)
	 {
		D3DVertexDeclaration_Release(g_pGradientVertexDecl);
		g_pGradientVertexDecl = NULL;
	 }

	 if (g_pGradientVertexShader)
	 {
		D3DVertexShader_Release(g_pGradientVertexShader);
		g_pGradientVertexShader = NULL;
	 }

	 if (g_pGradientVertexShader)
	 {
	    D3DVertexShader_Release(g_pGradientVertexShader);		 
		g_pGradientVertexShader = NULL;

	 }

	 if (g_pPixelShader)
	 {
		 D3DPixelShader_Release(g_pPixelShader);
		 g_pPixelShader = NULL;
	 }

	 if (vertexBuffer)
	 {
		 D3DVertexBuffer_Release(vertexBuffer);
		 vertexBuffer = NULL;
	 }

	if (vertexDeclaration)
	{
		D3DVertexDeclaration_Release(vertexDeclaration);
		vertexDeclaration = NULL;
	}

 	this->screen->pixels = NULL;
	 
}

static int XBOX_SetHWAlpha(_THIS, SDL_Surface *surface, Uint8 alpha)
{
	return(1);
}

static int XBOX_SetHWColorKey(_THIS, SDL_Surface *surface, Uint32 key)
{
	
	return(0);
}

static int XBOX_SetFlickerFilter(_THIS, SDL_Surface *surface, int filter)
{
 
	return(0);
}

static int XBOX_SetSoftDisplayFilter(_THIS, SDL_Surface *surface, int enabled)
{
 
	return(0);
}


static LPDIRECT3DTEXTURE9 CreateYUVSurface(_THIS, int width, int height, Uint32 format)
{
    LPDIRECT3DTEXTURE9 surface;

	IDirect3DDevice9_CreateTexture(D3D_Device,width,height,1, 0,D3DFMT_LIN_UYVY, D3DUSAGE_CPU_CACHED_MEMORY, (D3DTexture**)&surface, NULL);

	return surface;
}
 
SDL_Overlay *XBOX_CreateYUVOverlay(_THIS, int width, int height, Uint32 format, SDL_Surface *display)
{
	SDL_Overlay *overlay;
	struct private_yuvhwdata *hwdata;

	if (format == SDL_YV12_OVERLAY || format == SDL_IYUV_OVERLAY)
		return NULL;

	/* Create the overlay structure */
	overlay = (SDL_Overlay *)malloc(sizeof *overlay);
	if ( overlay == NULL ) {
		SDL_OutOfMemory();
		return(NULL);
	}
	memset(overlay, 0, (sizeof *overlay));

	/* Fill in the basic members */
	overlay->format = format;
	overlay->w = width;
	overlay->h = height;

	/* Set up the YUV surface function structure */
	overlay->hwfuncs = &XBOX_yuvfuncs;

	/* Create the pixel data and lookup tables */
	hwdata = (struct private_yuvhwdata *)malloc(sizeof *hwdata);
	overlay->hwdata = hwdata;
	if ( hwdata == NULL ) {
		SDL_OutOfMemory();
		SDL_FreeYUVOverlay(overlay);
		return(NULL);
	}
	hwdata->surface = CreateYUVSurface(this, width, height, format);
	if ( hwdata->surface == NULL ) {
		SDL_FreeYUVOverlay(overlay);
		return(NULL);
	}
	overlay->hw_overlay = 1;

	/* Set up the plane pointers */
	overlay->pitches = hwdata->pitches;
	overlay->pixels = hwdata->planes;
	switch (format) {
		case SDL_YV12_OVERLAY:
		case SDL_IYUV_OVERLAY:
		overlay->planes = 3;
		break;
		default:
		overlay->planes = 1;
		break;
	}

	/* We're all done.. */
	return(overlay);
 
}

int XBOX_DisplayYUVOverlay(_THIS, SDL_Overlay *overlay, SDL_Rect *src, SDL_Rect *dst)
{

	// this is slow. need to optimize
	
	LPDIRECT3DTEXTURE9 surface;
	D3DLOCKED_RECT destSurface;
	D3DLOCKED_RECT srcSurface;
	XGTEXTURE_DESC descSrc;
	XGTEXTURE_DESC descDst;
    
	RECT srcrect, dstrect;

	POINT dstPoint =
    {
        0, 0
    };
		
	surface = overlay->hwdata->surface;
	srcrect.top = src->y;
	srcrect.bottom = srcrect.top+src->h;
	srcrect.left = src->x;
	srcrect.right = srcrect.left+src->w;
	dstrect.top = dst->y;
	dstrect.left = dst->x;
	dstrect.bottom = dstrect.top+dst->h;
	dstrect.right = dstrect.left+dst->w;
 
    // Copy tiled texture to a linear texture
   
	IDirect3DTexture9_LockRect(surface, 0, &srcSurface, &srcrect, D3DLOCK_READONLY);
	IDirect3DTexture9_LockRect(this->hidden->SDL_primary, 0, &destSurface, NULL, D3DLOCK_NOOVERWRITE);
     
	XGGetTextureDesc( (D3DBaseTexture *)this->hidden->SDL_primary, 0, &descDst );
	XGGetTextureDesc( (D3DBaseTexture *)surface, 0, &descSrc );
	XGCopySurface( destSurface.pBits, destSurface.Pitch, dst->w, dst->h, descDst.Format, NULL,
                   srcSurface.pBits, srcSurface.Pitch, descSrc.Format, NULL, XGCOMPRESS_YUV_SOURCE , 0 );


    IDirect3DTexture9_UnlockRect(this->hidden->SDL_primary, 0);
	IDirect3DTexture9_UnlockRect(surface, 0);

	IDirect3DDevice9_DrawPrimitive(D3D_Device,  D3DPT_TRIANGLESTRIP, 0, 2 ); 
	IDirect3DDevice9_Present(D3D_Device,NULL,NULL,NULL,NULL);	 

	return 0;
}
int XBOX_LockYUVOverlay(_THIS, SDL_Overlay *overlay)
{	
 	LPDIRECT3DTEXTURE9 surface;
	D3DLOCKED_RECT rect;
 	
	surface = overlay->hwdata->surface;
		
	IDirect3DTexture9_LockRect(surface, 0, &rect, NULL, 0);

	/* Find the pitch and offset values for the overlay */
	overlay->pitches[0] = (Uint16)rect.Pitch;
	overlay->pixels[0]  = (Uint8 *)rect.pBits;
	switch (overlay->format) {
	    case SDL_YV12_OVERLAY:
	    case SDL_IYUV_OVERLAY:
		/* Add the two extra planes */
        overlay->pitches[0] = overlay->w;
		overlay->pitches[1] = overlay->pitches[0] / 2;
		overlay->pitches[2] = overlay->pitches[0] / 2;

		overlay->pixels[0] = (Uint8 *)rect.pBits;
	        overlay->pixels[1] = overlay->pixels[0] +
		                     overlay->pitches[0] * overlay->h;
	        overlay->pixels[2] = overlay->pixels[1] +
		                     overlay->pitches[1] * overlay->h / 2;

			overlay->planes = 3;
	        break;
	    default:
		/* Only one plane, no worries */
	break;
	}

	return 0;

}

void XBOX_UnlockYUVOverlay(_THIS, SDL_Overlay *overlay)
{

	LPDIRECT3DTEXTURE9 surface;

	surface = overlay->hwdata->surface;
	IDirect3DTexture9_UnlockRect(surface, 0);

}

static int XBOX_SetGammaRamp(_THIS, Uint16 *ramp)
{
	int i;
	int idx = 1;
	int scale = 256;
	D3DGAMMARAMP gammaRamp;

	if (ramp)
	{ 
		for (  i = 0 ;i < (256); i++)
		{
			gammaRamp.red[i] = ramp[i];
			gammaRamp.green[i] = ramp[i+256];
			gammaRamp.blue[i] = ramp[i+512];
		}
 
		/* Set up the gamma ramp */
		IDirect3DDevice9_SetGammaRamp(D3D_Device,0,D3DSGR_IMMEDIATE,&gammaRamp);	
 
	}
		 
	return 0;

}

void XBOX_FreeYUVOverlay(_THIS, SDL_Overlay *overlay)
{

	struct private_yuvhwdata *hwdata;

	hwdata = overlay->hwdata;
	if ( hwdata ) {
		if ( hwdata->surface ) {
			IDirect3DTexture9_Release(hwdata->surface);
		}
		free(hwdata);
		overlay->hwdata = NULL;
	}


}

