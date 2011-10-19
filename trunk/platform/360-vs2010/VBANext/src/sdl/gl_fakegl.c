/*
Quake source code is Copyright (C) 1996-1997 Id Software, Inc.
D3D8 FakeGL Wrapper is Copyright (C) 2009 MH

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// TODO Fix this warning instead of disabling it
#pragma warning (disable: 4273)

// we get a few signed/unsigned mismatches here
#pragma warning (disable: 4018)

/*
====================================================================================================================================

	NEW FAKE GL WRAPPER - MH 2009

	Uses Direct3D 8 for an (almost) seamless replacement of OpenGL.  Right now it's just restricted to the GL Calls used by
	Quake, but there's no reason why it can't be expanded if need be.

	It is assumed that your OpenGL code is reasonably correct so far as OpenGL is concerned, and therefore a certain layer
	of error checking and validation that *could* be included is actually omitted.  It is recommended that you test any
	OpenGL code you write using native OpenGL calls before sending it through this wrapper in order to ensure this
	correctness.

	Wherever possible the OpenGL specifications at http://www.opengl.org were used for guidance and interpretation of
	OpenGL calls.  The Direct3D 8 SDK help files and several sample aplications and other sources were used for the D3D
	portion of the code.  Every effort has been made to ensure that my interpretation of things is conformant with
	the published specs of each API.  It has been known for published specs to have bugs, and it has also been known
	for my interpretation to be incorrect, however; so let me know if there is anything that needs fixing.

	Compilation should be relatively seamless, although there are some changes you will need to make to your application
	in order to get the best experience from it.  These include the following items (listed with a GLQuake-derived engine
	in mind):

	- define USEFAKEGL somewhere in your code; I suggest either at the top of quakedef.h or glquake.h

	- remove the opengl32.lib and glu32.lib library imports from your linker options

	- replace the includes for gl.h and glu.h with something like the following:
		#ifndef USEFAKEGL
		#include <gl/gl.h>
		#include <GL/glu.h>
		#pragma comment (lib, "opengl32.lib")
		#pragma comment (lib, "glu32.lib")
		#else
		#include "fakegl.h"
		#endif

	- Replace the SwapBuffers call in gl_vidnt.c with FakeSwapBuffers

	- depending on your code you may also need to make certain other changes; see the examples given in sbar.c and
	  gl_screen.h for an idea of the kind of thing you might need (just search for #ifdef USEFAKEGL).

	YOU WILL NEED THE DIRECTX 8 SDK TO COMPILE THIS.

	It's still available on the internet (but not on microsoft.com) so just search on Google.

	It should compile OK on Visual C++ 2008 (including express) but there are a few steps to complete first.

	- Get an unmodified GLQuake compiling and running; there are tutorials on inside3d and/or quakeone for this.

	- Add the DirectX 8 SDK headers and libs to your Visual C++ directories.  I suggest you place them at the very end of the
	  list, as if you add them towards the top some defines or other files may conflict with the version of the platform SDK
	  used for this IDE.

	- create a fake libci.lib static library project (it can just contain one empty function called what you want, compile it,
	  and place the resulting .lib in your source directory so that d3dx8 can link OK.

====================================================================================================================================
*/



//#ifdef USEFAKEGL
#include <xtl.h>
#include <d3d9.h>
#include <d3dx9.h>
#include "fakegl.h"

 

static const CHAR* g_strVertexShader =

    
	"struct a2v { "
    "float4 Position : POSITION;"
	"float4 Normal   : NORMAL;"
	"float4 Color    : COLOR0; "
	"float2 UV       : TEXCOORD0; "
    "};"

    "struct v2p { "
    "float4 Position : POSITION; "
	"float4 Normal   : NORMAL;"
	"float4 Color    : COLOR0; "
	"float2 UV       : TEXCOORD0; "
    " }; "

    "void VShader(in a2v IN, out v2p OUT, "
       "     uniform float4x4 ModelViewMatrix) "
    "{"
    " OUT.Position = mul(IN.Position, ModelViewMatrix); "
    " OUT.Color    = IN.Color;"
	" IN.Normal.w  = 0.0; "
	" OUT.Normal   = normalize(IN.Normal);"
	" OUT.UV       = IN.UV;"
    "}";


D3DVERTEXELEMENT9 declOGL[] = 
{
{ 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
{ 0, 16, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
{ 0, 32, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
{ 0, 48, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
D3DDECL_END()
};
   
//-------------------------------------------------------------------------------------
// Pixel shader
//-------------------------------------------------------------------------------------
const char*                                 g_strPixelShader =
    " struct PS_IN                                 \n"
    " {                                            \n"
    "     float4 Color : COLOR;                    \n"  // Interpolated color from                      
    " };                                           \n"  // the vertex shader
    "                                              \n"
    " float4 main( PS_IN In ) : COLOR              \n"
    " {                                            \n"
    "     return In.Color;                         \n"  // Output color
    " }                                            \n";
 
// d3d basic stuff
LPDIRECT3D9 d3d_Object = NULL;
LPDIRECT3DDEVICE9 d3d_Device = NULL;
LPDIRECT3DVERTEXBUFFER9 vertexBuffer = NULL;
LPDIRECT3DVERTEXDECLARATION9 vertexDeclaration = NULL;
D3DVertexShader* g_pGradientVertexShader = NULL;
D3DPixelShader* g_pPixelShader = NULL;
LPD3DXBUFFER ppShader = NULL;
LPD3DXBUFFER pPixelShaderCode = NULL;
LPD3DXBUFFER pPixelErrorMsg = NULL;

// externs we need
void Sys_Error (char *error, ...) {};
void Con_Printf (char *fmt, ...) {};

// mode definition
int d3d_BPP = -1;
HWND d3d_Window;

BOOL d3d_RequestStencil = FALSE;

// the desktop and current display modes
D3DDISPLAYMODE d3d_DesktopMode;
D3DDISPLAYMODE d3d_CurrentMode;

// presentation parameters that the device was created with
D3DPRESENT_PARAMETERS d3d_PresentParams;

// capabilities
// these can exist for both the object and the device
D3DCAPS9 d3d_Caps;

// we're going to need this a lot so store it globally to save me having to redeclare it every time
HRESULT hr = S_OK;

// global state defines
DWORD d3d_ClearColor = 0x00000000;
BOOL d3d_DeviceLost = FALSE;
D3DVIEWPORT9 d3d_Viewport;

// utility
#define BYTE_CLAMP(i) (int) ((((i) > 255) ? 255 : (((i) < 0) ? 0 : (i))))

// go through the vtable rather than use the macros to make this generic
#define D3D_SAFE_RELEASE(p) {if (p) D3DResource_Release(p); (p) = NULL;}


DWORD GL_ColorToD3D (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	return D3DCOLOR_ARGB
	(
		BYTE_CLAMP (alpha * 255.0f),
		BYTE_CLAMP (red * 255.0f),
		BYTE_CLAMP (green * 255.0f),
		BYTE_CLAMP (blue * 255.0f)
	);
}


typedef struct d3d_texture_s
{
	// OpenGL number for glBindTexture/glGenTextures/glDeleteTextures/etc
	int glnum;

	// format the texture was uploaded as; 1 or 4 bytes.
	// glTexSubImage2D needs this.
	GLenum internalformat;

	// parameters
	DWORD addressu;
	DWORD addressv;
	DWORD anisotropy;
	DWORD magfilter;
	DWORD minfilter;
	DWORD mipfilter;

	// the texture image itself
	LPDIRECT3DTEXTURE9 teximg;

	struct d3d_texture_s *next;
} d3d_texture_t;

d3d_texture_t *d3d_Textures = NULL;

int d3d_TextureExtensionNumber = 1;

// opengl specified up to 32 TMUs, D3D only allows up to 8 stages
#define D3D_MAX_TMUS	8

typedef struct gl_combine_s
{
	DWORD colorop;
	DWORD colorarg1;
	DWORD colorarg2;
	DWORD alphaop;
	DWORD alphaarg1;
	DWORD alphaarg2;
	DWORD colorarg0;
	DWORD alphaarg0;
	DWORD resultarg;
	DWORD colorscale;
	DWORD alphascale;
} gl_combine_t;

typedef struct gl_tmustate_s
{
	d3d_texture_t *boundtexture;
	BOOL enabled;
	// BOOL combine;

	// gl_combine_t combstate;

	// these come from glTexEnv and are properties of the TMU; other D3DTEXTURESTAGESTATETYPEs 
	// come from glTexParameter and are properties of the individual texture.
	DWORD colorop;
	DWORD colorarg1;
	DWORD colorarg2;
	DWORD alphaop;
	DWORD alphaarg1;
	DWORD alphaarg2;
	DWORD texcoordindex;

	BOOL texenvdirty;
	BOOL texparamdirty;
} gl_tmustate_t;

gl_tmustate_t d3d_TMUs[D3D_MAX_TMUS];

int d3d_CurrentTMU = 0;

// used in various places
int D3D_TMUForTexture (GLenum texture)
{
	switch (texture)
	{
	case GLD3D_TEXTURE0: return 0;
	case GLD3D_TEXTURE1: return 1;
	case GLD3D_TEXTURE2: return 2;
	case GLD3D_TEXTURE3: return 3;
	case GLD3D_TEXTURE4: return 4;
	case GLD3D_TEXTURE5: return 5;
	case GLD3D_TEXTURE6: return 6;
	case GLD3D_TEXTURE7: return 7;

	default:
		// ? how best to fail gracefully (if we should fail gracefully at all?)
		return 0;
	}
}

// backface culling
// (fix me - put all of these into a state struct)
GLboolean gl_CullEnable = GL_TRUE;
GLenum gl_CullMode = GL_BACK;
GLenum gl_FrontFace = GL_CCW;

/*
===================================================================================================================

			HINT MECHANISM

	D3D generally doesn't provide a hint mechanism, instead relying on the programmer to explicitly set
	parameters for stuff.  In cases where certain items are in doubt we add hint capability.

===================================================================================================================
*/

// default to per-pixel fog
GLenum d3d_Fog_Hint = GL_NICEST;

void glHint (GLenum target, GLenum mode)
{
	switch (target)
	{
	case GL_FOG_HINT:
		d3d_Fog_Hint = mode;
		break;

	default:
		// ignore other driver hints
		break;
	}
}


/*
===================================================================================================================

			MATRIXES

	Most matrix ops occur in software in D3D; the only part that may (or may not) occur in hardware (depending
	on whether or not we have a hardware vp device) is the final transformation of submitted verts.  Note that
	OpenGL is probably the same.

	This interface just models the most common OpenGL matrix ops with their D3D equivalents.  I don't use the
	IDirect3DXMatrixStack interface because of overhead and baggage associated with it.

	The critical differences are in glGetFloatv and glLoadMatrix; OpenGL uses column major ordering whereas
	D3D uses row major ordering, so we need to translate from one order to the next.

	Also be aware that D3D defines separate world and view matrixes whereas OpenGL concatenates them into a
	single modelview matrix.  To keep things clean and consistent we will always use an identity view matrix
	on a beginscene and will just work off the world matrix.

	Finally, D3D does weird and horrible things in the texture matrix which are better left unsaid for now.

===================================================================================================================
*/

// OpenGL only has a stack depth of 2 for projection, but no reason to not use the full depth
#define MAX_MATRIX_STACK	32

typedef struct d3d_matrix_s
{
	BOOL dirty;
	int stackdepth;
	D3DTRANSFORMSTATETYPE usage;
	D3DMATRIX stack[MAX_MATRIX_STACK];
} d3d_matrix_t;

// init all matrixes to be dirty and at depth 0 (these will be re-inited each frame)
d3d_matrix_t d3d_ModelViewMatrix = {TRUE, 0, D3DTS_WORLD};
d3d_matrix_t d3d_ProjectionMatrix = {TRUE, 0, D3DTS_PROJECTION};

d3d_matrix_t *d3d_CurrentMatrix = &d3d_ModelViewMatrix;


void D3D_InitializeMatrix (d3d_matrix_t *m)
{
	// initializes a matrix to a known state prior to rendering
	m->dirty = TRUE;
	m->stackdepth = 0;
	D3DXMatrixIdentity (&m->stack[0]);
}


void D3D_CheckDirtyMatrix (d3d_matrix_t *m)
{
	if (m->dirty)
	{
		//IDirect3DDevice9_SetTransform (d3d_Device, m->usage, &m->stack[m->stackdepth]);
		m->dirty = FALSE;
	}
}


void glMatrixMode (GLenum mode)
{
	switch (mode)
	{
	case GL_MODELVIEW:
		d3d_CurrentMatrix = &d3d_ModelViewMatrix;
		break;

	case GL_PROJECTION:
		d3d_CurrentMatrix = &d3d_ProjectionMatrix;
		break;

	default:
		Sys_Error ("glMatrixMode: unimplemented mode");
		break;
	}
}

void gluLookAt	(	GLdouble eyeX , GLdouble eyeY , GLdouble eyeZ , GLdouble centerX , GLdouble centerY , GLdouble centerZ , GLdouble upX , GLdouble upY , GLdouble upZ )
{
	D3DXVECTOR3 eye;	
	D3DXVECTOR3 center;
	D3DXVECTOR3 up;


	eye.x = eyeX;
	eye.y = eyeY;
	eye.z = eyeZ;

	center.x = centerX;
	center.y = centerY;
	center.z = centerZ;

	up.x = upX;
	up.y = upY;
	up.z = upZ;


	D3DXMatrixLookAtRH(&d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth], &eye, &center, &up);
}


void glLoadIdentity (void)
{
	D3DXMatrixIdentity (&d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth]);
	d3d_CurrentMatrix->dirty = TRUE;
}


void glLoadMatrixf (const GLfloat *m)
{
	memcpy (d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth].m, m, sizeof (float) * 16);
	d3d_CurrentMatrix->dirty = TRUE;
}


void glFrustum (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	D3DMATRIX tmp;

	// per spec, glFrustum multiplies the current matrix by the specified orthographic projection rather than replacing it
	D3DXMatrixPerspectiveOffCenterRH (&tmp, left, right, bottom, top, zNear, zFar);

	D3DXMatrixMultiply (&d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth], &tmp, &d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth]);
	d3d_CurrentMatrix->dirty = TRUE;
}


void glOrtho (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	D3DMATRIX tmp;

	// per spec, glOrtho multiplies the current matrix by the specified orthographic projection rather than replacing it
	D3DXMatrixOrthoOffCenterRH (&tmp, left, right, bottom, top, zNear, zFar);

	D3DXMatrixMultiply (&d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth], &tmp, &d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth]);
	d3d_CurrentMatrix->dirty = TRUE;
}


void glPopMatrix (void)
{
	if (!d3d_CurrentMatrix->stackdepth)
	{
		// opengl silently allows this and so should we (witness TQ's R_DrawAliasModel, which pushes the
		// matrix once but pops it twice - on line 423 and line 468
		d3d_CurrentMatrix->dirty = TRUE;
		return;
	}

	// go to a new matrix
	d3d_CurrentMatrix->stackdepth--;

	// flag as dirty
	d3d_CurrentMatrix->dirty = TRUE;
}


void glPushMatrix (void)
{
	if (d3d_CurrentMatrix->stackdepth <= (MAX_MATRIX_STACK - 1))
	{
		// go to a new matrix (only push if there's room to push)
		d3d_CurrentMatrix->stackdepth++;

		// copy up the previous matrix (per spec)
		memcpy
		(
			&d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth],
			&d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth - 1],
			sizeof (D3DMATRIX)
		);
	}

	// flag as dirty
	d3d_CurrentMatrix->dirty = TRUE;
}


void glRotatef (GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	// replicates the OpenGL glRotatef with 3 components and angle in degrees
	D3DXVECTOR3 vec;
	D3DMATRIX tmp;

	vec.x = x;
	vec.y = y;
	vec.z = z;

	D3DXMatrixRotationAxis (&tmp, &vec, D3DXToRadian (angle));
	D3DXMatrixMultiply (&d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth], &tmp, &d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth]);

	// dirty the matrix
	d3d_CurrentMatrix->dirty = TRUE;
}


void glScalef (GLfloat x, GLfloat y, GLfloat z)
{
	D3DMATRIX tmp;
	D3DXMatrixScaling (&tmp, x, y, z);
	D3DXMatrixMultiply (&d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth], &tmp, &d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth]);

	// dirty the matrix
	d3d_CurrentMatrix->dirty = TRUE;
}


void glTranslatef (GLfloat x, GLfloat y, GLfloat z)
{
	D3DMATRIX tmp;
	D3DXMatrixTranslation (&tmp, x, y, z);
	D3DXMatrixMultiply (&d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth], &tmp, &d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth]);

	// dirty the matrix
	d3d_CurrentMatrix->dirty = TRUE;
}


void glMultMatrixf (const GLfloat *m)
{
	D3DMATRIX mat;

	// copy out
	mat._11 = m[0];
	mat._12 = m[1];
	mat._13 = m[2];
	mat._14 = m[3];

	mat._21 = m[4];
	mat._22 = m[5];
	mat._23 = m[6];
	mat._24 = m[7];

	mat._31 = m[8];
	mat._32 = m[9];
	mat._33 = m[10];
	mat._34 = m[11];

	mat._41 = m[12];
	mat._42 = m[13];
	mat._43 = m[14];
	mat._44 = m[15];

	D3DXMatrixMultiply (&d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth], &mat, &d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth]);

	// dirty the matrix
	d3d_CurrentMatrix->dirty = TRUE;
}


/*
===================================================================================================================

			STATE MANAGEMENT

	The D3D runtime will filter states for us anyway as we have selected a non-pure device (intentional even if
	a pure device is available as we need to support certain glGet functions); this just keeps the filtering local

===================================================================================================================
*/

LPDIRECT3DTEXTURE9 d3d_BoundTextures[D3D_MAX_TMUS];

void D3D_SetTexture (int stage, LPDIRECT3DTEXTURE9 texture)
{
	if (d3d_BoundTextures[stage] != texture)
	{
		d3d_BoundTextures[stage] = texture;
		IDirect3DDevice9_SetTexture (d3d_Device, stage, (IDirect3DBaseTexture9 *) texture);
	}
}


void D3D_DirtyAllStates (void)
{
	int i;

	// dirty TMUs
	for (i = 0; i < D3D_MAX_TMUS; i++)
	{
		d3d_TMUs[i].texenvdirty = TRUE;
		d3d_TMUs[i].texparamdirty = TRUE;
		d3d_BoundTextures[i] = NULL;
	}

	// and these as well
	d3d_ModelViewMatrix.dirty = TRUE;
	d3d_ProjectionMatrix.dirty = TRUE;
}


// d3d8 specifies 174 render states; here we just provide headroom
DWORD d3d_RenderStates[256];

// 28 stage states per stage
DWORD d3d_TextureStates[D3D_MAX_TMUS][32];

DWORD D3D_FloatToDWORD (float f)
{
	return ((DWORD *) &f)[0];
}


void D3D_SetRenderState (D3DRENDERSTATETYPE state, DWORD value)
{
	// filter state
	if (d3d_RenderStates[(int) state] == value) return;

	// set the state and cache it back
	IDirect3DDevice9_SetRenderState (d3d_Device, state, value);
	d3d_RenderStates[(int) state] = value;
}


void D3D_SetTextureState (DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{

}
 
void D3D_InitTexture (d3d_texture_t *tex)
{
	tex->glnum = 0;

	// note - we can't set the ->next pointer to NULL here as this texture may be a 
	// member of the linked list that has just become free...
	tex->addressu = D3DTADDRESS_WRAP;
	tex->addressv = D3DTADDRESS_WRAP;
	tex->magfilter = D3DTEXF_LINEAR;
	tex->minfilter = D3DTEXF_LINEAR;
	tex->mipfilter = D3DTEXF_LINEAR;
	tex->anisotropy = 1;

 
	D3D_SAFE_RELEASE (tex->teximg);
}


void D3D_GetRenderStates (void)
{
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_ZENABLE, &d3d_RenderStates[D3DRS_ZENABLE]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_FILLMODE, &d3d_RenderStates[D3DRS_FILLMODE]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_ZWRITEENABLE, &d3d_RenderStates[D3DRS_ZWRITEENABLE]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_ALPHATESTENABLE, &d3d_RenderStates[D3DRS_ALPHATESTENABLE]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_LASTPIXEL, &d3d_RenderStates[D3DRS_LASTPIXEL]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_SRCBLEND, &d3d_RenderStates[D3DRS_SRCBLEND]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_DESTBLEND, &d3d_RenderStates[D3DRS_DESTBLEND]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_CULLMODE, &d3d_RenderStates[D3DRS_CULLMODE]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_ZFUNC, &d3d_RenderStates[D3DRS_ZFUNC]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_ALPHAREF, &d3d_RenderStates[D3DRS_ALPHAREF]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_ALPHAFUNC, &d3d_RenderStates[D3DRS_ALPHAFUNC]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_ALPHABLENDENABLE, &d3d_RenderStates[D3DRS_ALPHABLENDENABLE]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_STENCILENABLE, &d3d_RenderStates[D3DRS_STENCILENABLE]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_STENCILFAIL, &d3d_RenderStates[D3DRS_STENCILFAIL]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_STENCILZFAIL, &d3d_RenderStates[D3DRS_STENCILZFAIL]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_STENCILPASS, &d3d_RenderStates[D3DRS_STENCILPASS]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_STENCILFUNC, &d3d_RenderStates[D3DRS_STENCILFUNC]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_STENCILREF, &d3d_RenderStates[D3DRS_STENCILREF]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_STENCILMASK, &d3d_RenderStates[D3DRS_STENCILMASK]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_STENCILWRITEMASK, &d3d_RenderStates[D3DRS_STENCILWRITEMASK]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_WRAP0, &d3d_RenderStates[D3DRS_WRAP0]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_WRAP1, &d3d_RenderStates[D3DRS_WRAP1]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_WRAP2, &d3d_RenderStates[D3DRS_WRAP2]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_WRAP3, &d3d_RenderStates[D3DRS_WRAP3]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_WRAP4, &d3d_RenderStates[D3DRS_WRAP4]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_WRAP5, &d3d_RenderStates[D3DRS_WRAP5]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_WRAP6, &d3d_RenderStates[D3DRS_WRAP6]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_WRAP7, &d3d_RenderStates[D3DRS_WRAP7]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_CLIPPLANEENABLE, &d3d_RenderStates[D3DRS_CLIPPLANEENABLE]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_POINTSIZE, &d3d_RenderStates[D3DRS_POINTSIZE]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_POINTSIZE_MIN, &d3d_RenderStates[D3DRS_POINTSIZE_MIN]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_POINTSPRITEENABLE, &d3d_RenderStates[D3DRS_POINTSPRITEENABLE]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_MULTISAMPLEANTIALIAS, &d3d_RenderStates[D3DRS_MULTISAMPLEANTIALIAS]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_MULTISAMPLEMASK, &d3d_RenderStates[D3DRS_MULTISAMPLEMASK]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_POINTSIZE_MAX, &d3d_RenderStates[D3DRS_POINTSIZE_MAX]);
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_COLORWRITEENABLE, &d3d_RenderStates[D3DRS_COLORWRITEENABLE]); 
	IDirect3DDevice9_GetRenderState (d3d_Device, D3DRS_BLENDOP, &d3d_RenderStates[D3DRS_BLENDOP]);

}


void D3D_SetRenderStates (void)
{
	// this forces a set of all render states will the current cached values rather than filtering, to
	// ensure that they are properly updated
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_ZENABLE, d3d_RenderStates[D3DRS_ZENABLE]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_FILLMODE, d3d_RenderStates[D3DRS_FILLMODE]);
//	IDirect3DDevice8_SetRenderState (d3d_Device, D3DRS_SHADEMODE, d3d_RenderStates[D3DRS_SHADEMODE]);
//	IDirect3DDevice8_SetRenderState (d3d_Device, D3DRS_LINEPATTERN, d3d_RenderStates[D3DRS_LINEPATTERN]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_ZWRITEENABLE, d3d_RenderStates[D3DRS_ZWRITEENABLE]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_ALPHATESTENABLE, d3d_RenderStates[D3DRS_ALPHATESTENABLE]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_LASTPIXEL, d3d_RenderStates[D3DRS_LASTPIXEL]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_SRCBLEND, d3d_RenderStates[D3DRS_SRCBLEND]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_DESTBLEND, d3d_RenderStates[D3DRS_DESTBLEND]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_CULLMODE, d3d_RenderStates[D3DRS_CULLMODE]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_ZFUNC, d3d_RenderStates[D3DRS_ZFUNC]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_ALPHAREF, d3d_RenderStates[D3DRS_ALPHAREF]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_ALPHAFUNC, d3d_RenderStates[D3DRS_ALPHAFUNC]);
//	IDirect3DDevice8_SetRenderState (d3d_Device, D3DRS_DITHERENABLE, d3d_RenderStates[D3DRS_DITHERENABLE]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_ALPHABLENDENABLE, d3d_RenderStates[D3DRS_ALPHABLENDENABLE]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_STENCILENABLE, d3d_RenderStates[D3DRS_STENCILENABLE]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_STENCILFAIL, d3d_RenderStates[D3DRS_STENCILFAIL]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_STENCILZFAIL, d3d_RenderStates[D3DRS_STENCILZFAIL]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_STENCILPASS, d3d_RenderStates[D3DRS_STENCILPASS]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_STENCILFUNC, d3d_RenderStates[D3DRS_STENCILFUNC]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_STENCILREF, d3d_RenderStates[D3DRS_STENCILREF]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_STENCILMASK, d3d_RenderStates[D3DRS_STENCILMASK]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_STENCILWRITEMASK, d3d_RenderStates[D3DRS_STENCILWRITEMASK]);
//	IDirect3DDevice8_SetRenderState (d3d_Device, D3DRS_TEXTUREFACTOR, d3d_RenderStates[D3DRS_TEXTUREFACTOR]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_WRAP0, d3d_RenderStates[D3DRS_WRAP0]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_WRAP1, d3d_RenderStates[D3DRS_WRAP1]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_WRAP2, d3d_RenderStates[D3DRS_WRAP2]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_WRAP3, d3d_RenderStates[D3DRS_WRAP3]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_WRAP4, d3d_RenderStates[D3DRS_WRAP4]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_WRAP5, d3d_RenderStates[D3DRS_WRAP5]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_WRAP6, d3d_RenderStates[D3DRS_WRAP6]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_WRAP7, d3d_RenderStates[D3DRS_WRAP7]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_CLIPPLANEENABLE, d3d_RenderStates[D3DRS_CLIPPLANEENABLE]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_POINTSIZE, d3d_RenderStates[D3DRS_POINTSIZE]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_POINTSIZE_MIN, d3d_RenderStates[D3DRS_POINTSIZE_MIN]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_POINTSPRITEENABLE, d3d_RenderStates[D3DRS_POINTSPRITEENABLE]);
 	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_MULTISAMPLEANTIALIAS, d3d_RenderStates[D3DRS_MULTISAMPLEANTIALIAS]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_MULTISAMPLEMASK, d3d_RenderStates[D3DRS_MULTISAMPLEMASK]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_POINTSIZE_MAX, d3d_RenderStates[D3DRS_POINTSIZE_MAX]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_COLORWRITEENABLE, d3d_RenderStates[D3DRS_COLORWRITEENABLE]);
	IDirect3DDevice9_SetRenderState (d3d_Device, D3DRS_BLENDOP, d3d_RenderStates[D3DRS_BLENDOP]);
 }


void D3D_GetTextureStates (void)
{
 
}


void D3D_SetTextureStates (void)
{
 
}


void D3D_InitStates (void)
{
	int i;

	// init tmus
	for (i = 0; i < D3D_MAX_TMUS; i++)
	{
		d3d_TMUs[i].boundtexture = NULL;
		d3d_TMUs[i].enabled = FALSE;
		d3d_TMUs[i].texcoordindex = i;
	}

	// store out all states
	D3D_GetRenderStates ();
	D3D_GetTextureStates ();

	// force all states to dirty on entry
	D3D_DirtyAllStates ();
}


D3DTEXTUREOP D3D_DecodeOp (D3DTEXTUREOP opin, DWORD scale)
{
	if (scale == 1)
		return opin;
	else if (scale == 2)
	{
		if (opin == D3DTOP_MODULATE)
			return D3DTOP_MODULATE2X;
		else if (opin == D3DTOP_ADDSIGNED)
			return D3DTOP_ADDSIGNED2X;
		else return opin;
	}
	else
	{
		if (opin == D3DTOP_MODULATE)
			return D3DTOP_MODULATE4X;
		else return opin;
	}
}


void D3D_CheckDirtyTextureStates (int tmu)
{
 
}


/*
===================================================================================================================

			POLYGON OFFSET

	Who said D3D didn't have polygon offset?

	It's implementation is quite a bit simpler than OpenGL's however; just a pretty basic static bias
	(it's better in D3D9)

===================================================================================================================
*/

BOOL d3d_PolyOffsetEnabled = FALSE;
BOOL d3d_PolyOffsetSwitched = FALSE;
float d3d_PolyOffsetFactor = 8;

void glPolygonOffset (GLfloat factor, GLfloat units)
{
	// need to switch polygon offset
	d3d_PolyOffsetSwitched = FALSE;

	// just use the units here as we're going to fudge it using D3DRS_ZBIAS
	// 0 is furthest; 16 is nearest; d3d default is 0 so our default is an intermediate value (8)
	// so that we can do both push back and pull forward
	// negative values come nearer; positive values go further, so invert the sense
	d3d_PolyOffsetFactor = 8 - units;

	// clamp to d3d scale
	if (d3d_PolyOffsetFactor < 0) d3d_PolyOffsetFactor = 0;
	if (d3d_PolyOffsetFactor > 16) d3d_PolyOffsetFactor = 16;
}


/*
===================================================================================================================

			VERTEX SUBMISSION

	Per the spec for glVertex (http://www.opengl.org/sdk/docs/man/xhtml/glVertex.xml) glColor, glNormal and
	glTexCoord just specify a *current* color, normal and texcoord, whereas glVertex actually creates a vertex
	that inherits the current color, normal and texcoord.

	Real D3D code looks *nothing* like this.

===================================================================================================================
*/

int d3d_PrimitiveMode = 0;
int d3d_NumVerts = 0;

// this should be a multiple of 12 to support both GL_QUADS and GL_TRIANGLES
// it should also be large enough to hold the biggest tristrip or fan in use in the engine
// individual quads or tris can be submitted in batches
#define D3D_MAX_VERTEXES	256

typedef struct gl_texcoord_s
{
	float s;
	float t;
} gl_texcoord_t;

typedef struct gl_xyz_s
{
	float x;
	float y;
	float z;
} gl_xyz_t;

// defaults that are picked up by each glVertex call
D3DCOLOR d3d_CurrentColor = 0xffffffff;
gl_texcoord_t d3d_CurrentTexCoord[D3D_MAX_TMUS] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}};
gl_xyz_t d3d_CurrentNormal = {0, 1, 0};

// this may be a little wasteful as it's a full sized vertex for 8 TMUs
// we'll fix it if it becomes a problem (it's not like Quake stresses the GPU too much anyway)
typedef struct gl_vertex_s
{
	gl_xyz_t position;
	gl_xyz_t normal;
	D3DCOLOR c;
	gl_texcoord_t st[D3D_MAX_TMUS];
} gl_vertex_t;

gl_vertex_t d3d_Vertexes[64];

BOOL d3d_SceneBegun = FALSE;

void GL_SubmitVertexes (void)
{
	int i;
	DWORD d3d_VertexShader;
	DWORD d3d_TexCoordSizes;
	DWORD d3d_TMUShader[] =
	{
		D3DFVF_TEX0,
		D3DFVF_TEX1,
		D3DFVF_TEX2,
		D3DFVF_TEX3,
		D3DFVF_TEX4,
		D3DFVF_TEX5,
		D3DFVF_TEX6,
		D3DFVF_TEX7,
		D3DFVF_TEX8
	};

	if (!d3d_Device) return;

	// check for a beginscene
	if (!d3d_SceneBegun)
	{
		// D3D has a separate view matrix which is concatenated with the world in OpenGL.  We maintain
		// a compatible interface by just setting the D3D view matrix to identity and doing all modelview
		// transforms via the world matrix.
		D3DMATRIX d3d_ViewMatrix;

		// issue a beginscene (geometry needs this
		IDirect3DDevice9_BeginScene (d3d_Device);

		// force an invalid vertex shader so that the first time will change it
		D3D_SetVertexShader ();

		// clear down bound textures
		for (i = 0; i < D3D_MAX_TMUS; i++) d3d_BoundTextures[i] = NULL;

		// we're in a scene now
		d3d_SceneBegun = TRUE;

		// now set our identity view matrix
		D3DXMatrixIdentity (&d3d_ViewMatrix);
		//IDirect3DDevice9_SetTransform (d3d_Device, D3DTS_VIEW, &d3d_ViewMatrix);
	}

	// check polygon offset
	if (!d3d_PolyOffsetSwitched)
	{
		if (d3d_PolyOffsetEnabled)
		{
			// setup polygon offset
			//D3D_SetRenderState (D3DRS_ZBIAS, d3d_PolyOffsetFactor);
		}
		else
		{
			// no polygon offset - back to normal z bias
			// (see comment in 
//			D3D_SetRenderState (D3DRS_ZBIAS, 8);
		}

		// we've switched polygon offset now
		d3d_PolyOffsetSwitched = TRUE;
	}

	// check for dirty matrixes
	D3D_CheckDirtyMatrix (&d3d_ModelViewMatrix);
	D3D_CheckDirtyMatrix (&d3d_ProjectionMatrix);

	// initial vertex shader (will be added to as TMUs accumulate)
	d3d_VertexShader = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE;
	d3d_TexCoordSizes = 0;

	// set up textures
	for (i = 0; i < D3D_MAX_TMUS; i++)
	{
		// end of TMUs
		if (!d3d_TMUs[i].enabled || !d3d_TMUs[i].boundtexture)
		{
			// explicitly disable alpha and color ops in this and subsequent stages
			D3D_SetTextureState (i, D3DTSS_COLOROP, D3DTOP_DISABLE);
			D3D_SetTextureState (i, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

			// done
			break;
		}

		// set up texcoord sizes
		d3d_TexCoordSizes |= D3DFVF_TEXCOORDSIZE2 (i);

		// check for dirty states (this is needed as OpenGL sets state per-texture as well as per-stage)
		D3D_CheckDirtyTextureStates (i);

		// bind the texture (now with added filtering!)
		D3D_SetTexture (i, d3d_TMUs[i].boundtexture->teximg);
	}

	// set the correct vertex shader for the number of enabled TMUs
	D3D_SetVertexShader ();


	// draw the verts - these are the only modes we support for Quake
	// per the spec, GL_FRONT_AND_BACK still allows lines and points through
	switch (d3d_PrimitiveMode)
	{
	case GL_TRIANGLES:
		// D3DPT_TRIANGLELIST models GL_TRIANGLES when used for either a single triangle or multiple triangles
		if (gl_CullMode != GL_FRONT_AND_BACK) IDirect3DDevice9_DrawPrimitiveUP (d3d_Device, D3DPT_TRIANGLELIST, d3d_NumVerts / 3, d3d_Vertexes, sizeof (gl_vertex_t));
		break;

	case GL_TRIANGLE_STRIP:
		// regular tristrip
		if (gl_CullMode != GL_FRONT_AND_BACK) IDirect3DDevice9_DrawPrimitiveUP (d3d_Device, D3DPT_TRIANGLESTRIP, d3d_NumVerts - 2, d3d_Vertexes, sizeof (gl_vertex_t));
		break;

	case GL_POLYGON:
		// a GL_POLYGON has the same vertex layout and order as a trifan, and can be used interchangably in OpenGL
	case GL_TRIANGLE_FAN:
		// regular trifan
		if (gl_CullMode != GL_FRONT_AND_BACK) IDirect3DDevice9_DrawPrimitiveUP (d3d_Device, D3DPT_TRIANGLEFAN, d3d_NumVerts - 2, d3d_Vertexes, sizeof (gl_vertex_t));
		break;

	case GL_QUADS:
		if (gl_CullMode == GL_FRONT_AND_BACK) break;

		// quads are a special case of trifans where each quad (numverts / 4) represents a trifan with 2 prims in it
		for (i = 0; i < d3d_NumVerts; i += 4)
			IDirect3DDevice9_DrawPrimitiveUP (d3d_Device, D3DPT_TRIANGLEFAN, 2, &d3d_Vertexes[i], sizeof (gl_vertex_t));

		break;

	case GL_LINES:
		//IDirect3DDevice9_DrawPrimitiveUP (d3d_Device, D3DPT_LINELIST, d3d_NumVerts, d3d_Vertexes, sizeof (gl_vertex_t));
		break;

	case GL_QUAD_STRIP:
		// not as optimal as it could be, so hopefully it won't be used too often!!!
		if (gl_CullMode == GL_FRONT_AND_BACK) break;

		for (i = 0; ; i += 2)
		{
			short quadindexes[4];

			if (i > (d3d_NumVerts - 3)) break;

			quadindexes[0] = i;
			quadindexes[1] = i + 1;
			quadindexes[2] = i + 3;
			quadindexes[3] = i + 2;

			IDirect3DDevice9_DrawIndexedPrimitiveUP
			(
				d3d_Device,
				D3DPT_TRIANGLEFAN,
				quadindexes[0],
				4,
				2,
				quadindexes,
				D3DFMT_INDEX16,
				d3d_Vertexes,
				sizeof (gl_vertex_t)
			);
		}

		break;

	default:
		// unsupported mode
		break;
	}

	// begin a new primitive
	d3d_NumVerts = 0;
}


void glVertex2fv (const GLfloat *v)
{
	glVertex3f (v[0], v[1], 0);
}


void glVertex2f (GLfloat x, GLfloat y)
{
	glVertex3f (x, y, 0);
}


void glVertex3fv (const GLfloat *v)
{
	glVertex3f (v[0], v[1], v[2]);
}


void glVertex3f (GLfloat x, GLfloat y, GLfloat z)
{
	int i;

	// add a new vertex to the list with the specified xyz and inheriting the current normal, color and texcoords
	// (per spec at http://www.opengl.org/sdk/docs/man/xhtml/glVertex.xml)
	d3d_Vertexes[d3d_NumVerts].position.x = x;
	d3d_Vertexes[d3d_NumVerts].position.y = y;
	d3d_Vertexes[d3d_NumVerts].position.z = z;

	d3d_Vertexes[d3d_NumVerts].normal.x = d3d_CurrentNormal.x;
	d3d_Vertexes[d3d_NumVerts].normal.y = d3d_CurrentNormal.y;
	d3d_Vertexes[d3d_NumVerts].normal.z = d3d_CurrentNormal.z;

	d3d_Vertexes[d3d_NumVerts].c = d3d_CurrentColor;

	for (i = 0; i < D3D_MAX_TMUS; i++)
	{
		d3d_Vertexes[d3d_NumVerts].st[i].s = d3d_CurrentTexCoord[i].s;
		d3d_Vertexes[d3d_NumVerts].st[i].t = d3d_CurrentTexCoord[i].t;
	}

	// go to a new vertex
	d3d_NumVerts++;

	// check for end of vertexes
	if (d3d_NumVerts == D3D_MAX_VERTEXES)
	{
		if (d3d_PrimitiveMode == GL_TRIANGLES || d3d_PrimitiveMode == GL_QUADS)
		{
			// triangles and quads are discrete primitives and as such can begin a new batch
			GL_SubmitVertexes ();
		}
		else
		{
			// other primitives need to extend the vertex storage
		}

		d3d_NumVerts = 0;
	}
}


void glTexCoord2f (GLfloat s, GLfloat t)
{
	d3d_CurrentTexCoord[0].s = s;
	d3d_CurrentTexCoord[0].t = t;
}


void glTexCoord2fv (const GLfloat *v)
{
	d3d_CurrentTexCoord[0].s = v[0];
	d3d_CurrentTexCoord[0].t = v[1];
}


void GL_SetColor (int red, int green, int blue, int alpha)
{
	// overwrite color incase verts set it
	d3d_CurrentColor = D3DCOLOR_ARGB
	(
		BYTE_CLAMP (alpha),
		BYTE_CLAMP (red),
		BYTE_CLAMP (green),
		BYTE_CLAMP (blue)
	);
}


void glColor3f (GLfloat red, GLfloat green, GLfloat blue)
{
	GL_SetColor (red * 255, green * 255, blue * 255, 255);
}


void glColor3fv (const GLfloat *v)
{
	GL_SetColor (v[0] * 255, v[1] * 255, v[2] * 255, 255);
}


void glColor3ubv (const GLubyte *v)
{
	GL_SetColor (v[0], v[1], v[2], 255);
}


void glColor4f (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	GL_SetColor (red * 255, green * 255, blue * 255, alpha * 255);
}


void glColor4fv (const GLfloat *v)
{
	GL_SetColor (v[0] * 255, v[1] * 255, v[2] * 255, v[3] * 255);
}


void glColor4ubv (const GLubyte *v)
{
	GL_SetColor (v[0], v[1], v[2], v[3]);
}


void glColor4ub (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
	GL_SetColor (red, green, blue, alpha);
}


void glNormal3f (GLfloat nx, GLfloat ny, GLfloat nz)
{
	d3d_CurrentNormal.x = nx;
	d3d_CurrentNormal.y = ny;
	d3d_CurrentNormal.z = nz;
}


void glBegin (GLenum mode)
{
	// just store out the mode, all heavy lifting is done in glEnd
	d3d_PrimitiveMode = mode;

	// begin a new primitive
	d3d_NumVerts = 0;
}


void glEnd (void)
{
	// submit, bitch
	GL_SubmitVertexes ();
}


/*
===================================================================================================================

			VERTEX ARRAYS

===================================================================================================================
*/

typedef struct gl_varray_pointer_s
{
	GLint size;
	GLenum type;
	GLsizei stride;
	GLvoid *pointer;
} gl_varray_pointer_t;

gl_varray_pointer_t d3d_VertexPointer;
gl_varray_pointer_t d3d_ColorPointer;
gl_varray_pointer_t d3d_TexCoordPointer[D3D_MAX_TMUS];
int d3d_VArray_TMU = 0;

void WINAPI GL_ClientActiveTexture (GLenum texture)
{
	d3d_VArray_TMU = D3D_TMUForTexture (texture);
}

void D3D_SetVertexShader ()
{

	// set and cache back

	void *pLockedVertexBuffer;

	 XMMATRIX matWorld = XMMatrixIdentity();
    // View matrix
    XMVECTOR vEyePt = XMVectorSet( 0.0f, -4.0f, -4.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    XMMATRIX matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUp );

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )640 / ( FLOAT )480;

    // Projection matrix
    XMMATRIX matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspectRatio, 1.0f, 200.0f );

    // World*view*projection
    XMMATRIX matWVP = XMMatrixMultiply(matProj, matView); 
	matWVP = XMMatrixMultiply(matWorld, matWVP); 
 
	//D3DMATRIX matWVP  = d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth];

	//IDirect3DVertexBuffer9_Lock(vertexBuffer, 0, 0, (BYTE **)&pLockedVertexBuffer, 0L );
	//memcpy(pLockedVertexBuffer,d3d_VertexPointer.pointer,sizeof(gl_varray_pointer_t));
	//IDirect3DVertexBuffer9_Unlock(vertexBuffer);
  
	
	//D3DDevice_SetRenderState(d3d_Device, D3DRS_VIEWPORTENABLE, FALSE);
}


void glEnableClientState (GLenum array)
{
	switch (array)
	{
	case GL_VERTEX_ARRAY:
	case GL_COLOR_ARRAY:
	case GL_TEXTURE_COORD_ARRAY:
		// doesn't need to do anything
		break;

	default:
		Sys_Error ("Invalid Vertex Array Spec...!");
	}
}


void glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
	int i;
	int v;
	int tmu;
	byte *vp;
	void *pLockedVertexBuffer;
	byte *stp[D3D_MAX_TMUS];

	// required by the spec
	if (!d3d_VertexPointer.pointer) return;

	vp = ((byte *) d3d_VertexPointer.pointer + first);

	for (tmu = 0; tmu < D3D_MAX_TMUS; tmu++)
	{
		if (d3d_TexCoordPointer[tmu].pointer)
			stp[tmu] = ((byte *) d3d_TexCoordPointer[tmu].pointer + first);
		else stp[tmu] = NULL;
	}

	// send through standard begin/end processing
	glBegin (mode);


    IDirect3DVertexBuffer9_Lock(vertexBuffer, 0, 0, (BYTE **)&pLockedVertexBuffer, 0L );
 
	for (i = 0, v = first; i < count; i++, v++)
	{
		for (tmu = 0; tmu < D3D_MAX_TMUS; tmu++)
		{
			if (stp[tmu])
			{
				d3d_CurrentTexCoord[tmu].s = ((float *) stp[tmu])[0];
				d3d_CurrentTexCoord[tmu].t = ((float *) stp[tmu])[1];

				stp[tmu] += d3d_TexCoordPointer[tmu].stride;
			}
		}

		if (d3d_VertexPointer.size == 2)
			glVertex2fv ((float *) vp);
		else if (d3d_VertexPointer.size == 3)
			glVertex3fv ((float *) vp);

		vp += d3d_VertexPointer.stride;
	}

	memcpy(pLockedVertexBuffer,d3d_Vertexes,sizeof(gl_varray_pointer_t));
	IDirect3DVertexBuffer9_Unlock(vertexBuffer);


	glEnd ();
}


void glVertexPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	if (type != GL_FLOAT) Sys_Error ("Unimplemented vertex pointer type");

	d3d_VertexPointer.size = size;
	d3d_VertexPointer.type = type;
	d3d_VertexPointer.stride = stride;
	d3d_VertexPointer.pointer = (GLvoid *) pointer;

}


void glColorPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	if (type != GL_FLOAT) Sys_Error ("Unimplemented color pointer type");

	d3d_ColorPointer.size = size;
	d3d_ColorPointer.type = type;
	d3d_ColorPointer.stride = stride;
	d3d_ColorPointer.pointer = (GLvoid *) pointer;
}


void glTexCoordPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	if (type != GL_FLOAT) Sys_Error ("Unimplemented texcoord pointer type");

	d3d_TexCoordPointer[d3d_VArray_TMU].size = size;
	d3d_TexCoordPointer[d3d_VArray_TMU].type = type;
	d3d_TexCoordPointer[d3d_VArray_TMU].stride = stride;
	d3d_TexCoordPointer[d3d_VArray_TMU].pointer = (GLvoid *) pointer;
}


void glDisableClientState (GLenum array)
{
	// switch the pointer to NULL
	switch (array)
	{
	case GL_VERTEX_ARRAY:
		d3d_VertexPointer.pointer = NULL;
		break;

	case GL_COLOR_ARRAY:
		d3d_ColorPointer.pointer = NULL;
		break;

	case GL_TEXTURE_COORD_ARRAY:
		d3d_TexCoordPointer[d3d_VArray_TMU].pointer = NULL;
		break;

	default:
		Sys_Error ("Invalid Vertex Array Spec...!");
	}
}


void glCopyTexImage2D (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {}
void glTexGeni (GLenum coord, GLenum pname, GLint param) {}
void glDeleteLists (GLuint list, GLsizei range) {}
GLuint glGenLists (GLsizei range) {return 1;}
void glNewList (GLuint list, GLenum mode) {}
void glEndList (void) {}
void glCallList (GLuint list) {}
void glTexImage1D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels) {}
void glLineWidth (GLfloat width) {}


/*
===================================================================================================================

			STENCIL BUFFER

===================================================================================================================
*/

void glStencilFunc (GLenum func, GLint ref, GLuint mask)
{
}


void glStencilOp (GLenum fail, GLenum zfail, GLenum zpass)
{
}


void glClearStencil (GLint s)
{
	IDirect3DDevice9_Clear (d3d_Device, 0, NULL, D3DCLEAR_STENCIL, 0x00000000, 1, 1);
}


/*
===================================================================================================================

			TEXTURES

===================================================================================================================
*/


d3d_texture_t *D3D_AllocTexture (void)
{
	d3d_texture_t *tex;

	// find a free texture
	for (tex = d3d_Textures; tex; tex = tex->next)
	{
		// if either of these are 0 (or NULL) we just reuse it
		if (!tex->teximg)
		{
			D3D_InitTexture (tex);
			return tex;
		}

		if (!tex->glnum)
		{
			D3D_InitTexture (tex);
			return tex;
		}
	}

	// nothing to reuse so create a new one
	// clear to 0 is required so that D3D_SAFE_RELEASE is valid
	tex = (d3d_texture_t *) malloc (sizeof (d3d_texture_t));
	memset (tex, 0, sizeof (d3d_texture_t));
	D3D_InitTexture (tex);

	// link in
	tex->next = d3d_Textures;
	d3d_Textures = tex;

	// return the new one
	return tex;
}


void D3D_ReleaseTextures (void)
{
	d3d_texture_t *tex;

	// explicitly NULL all textures and force texparams to dirty
	for (tex = d3d_Textures; tex; tex = tex->next)
	{
		D3D_InitTexture (tex);
	}
}


/*
======================
D3D_CheckTextureFormat

Ensures that a given texture format will be available
======================
*/
BOOL D3D_CheckTextureFormat (D3DFORMAT TextureFormat, D3DFORMAT AdapterFormat)
{
	hr = IDirect3D9_CheckDeviceFormat
	(
		d3d_Object,
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		AdapterFormat,
		0,
		D3DRTYPE_TEXTURE,
		TextureFormat
	);

	return SUCCEEDED (hr);
}


void glTexEnvf (GLenum target, GLenum pname, GLfloat param)
{
	if (target != GL_TEXTURE_ENV) Sys_Error ("glTexEnvf: unimplemented target");

	d3d_TMUs[d3d_CurrentTMU].texenvdirty = TRUE;

	switch (pname)
	{
	case GL_TEXTURE_ENV_MODE:
		// this is the default mode
		switch ((GLint) param)
		{
		case GLD3D_COMBINE:
			// flag a combine
			//d3d_TMUs[d3d_CurrentTMU].combine = TRUE;
			break;

		case GL_ADD:
			d3d_TMUs[d3d_CurrentTMU].colorop = D3DTOP_ADD;
			d3d_TMUs[d3d_CurrentTMU].colorarg1 = D3DTA_TEXTURE;
			d3d_TMUs[d3d_CurrentTMU].colorarg2 = (d3d_CurrentTMU == 0 ? D3DTA_DIFFUSE : D3DTA_CURRENT);

			d3d_TMUs[d3d_CurrentTMU].alphaop = D3DTOP_ADD;
			d3d_TMUs[d3d_CurrentTMU].alphaarg1 = D3DTA_TEXTURE;
			d3d_TMUs[d3d_CurrentTMU].alphaarg2 = D3DTA_DIFFUSE;

			//d3d_TMUs[d3d_CurrentTMU].combine = FALSE;
			break;

		case GL_REPLACE:
			// there was a reason why i changed this to modulate but i can't remember it :(
			// anyway, it needs to be D3DTOP_SELECTARG1 for Quake, so D3DTOP_SELECTARG1 it is...
			d3d_TMUs[d3d_CurrentTMU].colorop = D3DTOP_SELECTARG1;
			d3d_TMUs[d3d_CurrentTMU].colorarg1 = D3DTA_TEXTURE;
			d3d_TMUs[d3d_CurrentTMU].colorarg2 = D3DTA_DIFFUSE;

			d3d_TMUs[d3d_CurrentTMU].alphaop = D3DTOP_SELECTARG1;
			d3d_TMUs[d3d_CurrentTMU].alphaarg1 = D3DTA_TEXTURE;
			d3d_TMUs[d3d_CurrentTMU].alphaarg2 = D3DTA_DIFFUSE;

			//d3d_TMUs[d3d_CurrentTMU].combine = FALSE;
			break;

		case GL_MODULATE:
			d3d_TMUs[d3d_CurrentTMU].colorop = D3DTOP_MODULATE;
			d3d_TMUs[d3d_CurrentTMU].colorarg1 = D3DTA_TEXTURE;
			d3d_TMUs[d3d_CurrentTMU].colorarg2 = (d3d_CurrentTMU == 0 ? D3DTA_DIFFUSE : D3DTA_CURRENT);

			d3d_TMUs[d3d_CurrentTMU].alphaop = D3DTOP_MODULATE;
			d3d_TMUs[d3d_CurrentTMU].alphaarg1 = D3DTA_TEXTURE;
			d3d_TMUs[d3d_CurrentTMU].alphaarg2 = D3DTA_DIFFUSE;

			//d3d_TMUs[d3d_CurrentTMU].combine = FALSE;
			break;

		case GL_DECAL:
			d3d_TMUs[d3d_CurrentTMU].colorop = D3DTOP_BLENDTEXTUREALPHA;
			d3d_TMUs[d3d_CurrentTMU].colorarg1 = D3DTA_TEXTURE;
			d3d_TMUs[d3d_CurrentTMU].colorarg2 = (d3d_CurrentTMU == 0 ? D3DTA_DIFFUSE : D3DTA_CURRENT);

			d3d_TMUs[d3d_CurrentTMU].alphaop = D3DTOP_SELECTARG1;
			d3d_TMUs[d3d_CurrentTMU].alphaarg1 = D3DTA_TEXTURE;
			d3d_TMUs[d3d_CurrentTMU].alphaarg2 = D3DTA_DIFFUSE;

			//d3d_TMUs[d3d_CurrentTMU].combine = FALSE;
			break;

		case GL_BLEND:
			d3d_TMUs[d3d_CurrentTMU].colorop = D3DTOP_MODULATE;
			d3d_TMUs[d3d_CurrentTMU].colorarg1 = D3DTA_TEXTURE | D3DTA_COMPLEMENT;
			d3d_TMUs[d3d_CurrentTMU].colorarg2 = (d3d_CurrentTMU == 0 ? D3DTA_DIFFUSE : D3DTA_CURRENT);

			d3d_TMUs[d3d_CurrentTMU].alphaop = D3DTOP_SELECTARG1;
			d3d_TMUs[d3d_CurrentTMU].alphaarg1 = D3DTA_TEXTURE;
			d3d_TMUs[d3d_CurrentTMU].alphaarg2 = D3DTA_DIFFUSE;

			//d3d_TMUs[d3d_CurrentTMU].combine = FALSE;
			break;

		default:
			Sys_Error ("glTexEnvf: unimplemented param");
			break;
		}

		break;

	case GLD3D_COMBINE_RGB:
		//if ((int) param == GL_MODULATE)
		//	d3d_TMUs[d3d_CurrentTMU].combstate.colorop = D3DTOP_MODULATE;
		//else Sys_Error ("glTexEnvf: unimplemented param");
		break;

	case GLD3D_SOURCE0_RGB:
		//if ((int) param == GL_TEXTURE)
		//	d3d_TMUs[d3d_CurrentTMU].combstate.colorarg1 = D3DTA_TEXTURE;
		//else if ((int) param == GLD3D_PREVIOUS)
		//	d3d_TMUs[d3d_CurrentTMU].combstate.colorarg1 = D3DTA_CURRENT;
		//else Sys_Error ("glTexEnvf: unimplemented param");
		break;

	case GLD3D_SOURCE1_RGB:
		//if ((int) param == GL_TEXTURE)
		//	d3d_TMUs[d3d_CurrentTMU].combstate.colorarg2 = D3DTA_TEXTURE;
		//else if ((int) param == GLD3D_PRIMARY_COLOR)
		//	d3d_TMUs[d3d_CurrentTMU].combstate.colorarg2 = D3DTA_DIFFUSE;
		//else Sys_Error ("glTexEnvf: unimplemented param");
		break;

	case GLD3D_RGB_SCALE:
		// d3d only allows 1/2/4 x scale and in practice so do most OpenGL implementations
		// (this is actually required by the spec: see http://www.opengl.org/sdk/docs/man/xhtml/glTexEnv.xml)
		//if (param > 2)
		//	d3d_TMUs[d3d_CurrentTMU].combstate.colorscale = 4;
		//else if (param > 1)
		//	d3d_TMUs[d3d_CurrentTMU].combstate.colorscale = 2;
		//else d3d_TMUs[d3d_CurrentTMU].combstate.colorscale = 1;
		break;

	default:
		Sys_Error ("glTexEnvf: unimplemented pname");
		break;
	}
}


void glTexEnvi (GLenum target, GLenum pname, GLint param)
{
	if (target != GL_TEXTURE_ENV) return;

	glTexEnvf (target, pname, param);
}


void D3D_FillTextureLevel (LPDIRECT3DTEXTURE9 texture, int level, GLint internalformat, int width, int height, GLint format, const void *pixels)
{
	int i;
	int srcbytes = 0;
	int dstbytes = 0;
	byte *srcdata;
	byte *dstdata;
	D3DLOCKED_RECT lockrect;
  
	if (format == 1 || format == GL_LUMINANCE)
		srcbytes = 1;
	else if (format == 3 || format == GL_RGB)
		srcbytes = 3;
	else if (format == 4 || format == GL_RGBA ||  format == GL_RGBA8 ||  format==GL_BGRA_EXT)
		srcbytes = 4;
	else Sys_Error ("D3D_FillTextureLevel: illegal format");

	// d3d doesn't have an internal RGB only format
	// (neither do most OpenGL implementations, they just let you specify it as RGB but expand internally to 4 component)
	if (internalformat == 1 || internalformat == GL_LUMINANCE)
		dstbytes = 1;
	else if (internalformat == 3 || internalformat == GL_RGB)
		dstbytes = 4;
	else if (internalformat == 4 || internalformat == GL_RGBA  || format == GL_RGBA8 || format==GL_BGRA_EXT)
		dstbytes = 4;
	else Sys_Error ("D3D_FillTextureLevel: illegal internalformat");

	if (pixels==NULL)
		return;

	IDirect3DTexture9_LockRect (texture, level, &lockrect, NULL, 0);

	srcdata = (byte *) pixels;
	dstdata = lockrect.pBits;

	for (i = 0; i < width * height; i++)
	{
		if (srcbytes == 1)
		{
			if (dstbytes == 1)
				dstdata[0] = srcdata[0];
			else if (dstbytes == 4)
			{
				dstdata[0] = srcdata[0];
				dstdata[1] = srcdata[0];
				dstdata[2] = srcdata[0];
				dstdata[3] = srcdata[0];
			}
		}
		else if (srcbytes == 3)
		{
			if (dstbytes == 1)
				dstdata[0] = ((int) srcdata[0] + (int) srcdata[1] + (int) srcdata[2]) / 3;
			else if (dstbytes == 4)
			{
				dstdata[0] = srcdata[2];
				dstdata[1] = srcdata[1];
				dstdata[2] = srcdata[0];
				dstdata[3] = 255;
			}
		}
		else if (srcbytes == 4)
		{
			if (dstbytes == 1)
				dstdata[0] = ((int) srcdata[0] + (int) srcdata[1] + (int) srcdata[2]) / 3;
			else if (dstbytes == 4)
			{
				dstdata[0] = srcdata[2];
				dstdata[1] = srcdata[1];
				dstdata[2] = srcdata[0];
				dstdata[3] = srcdata[3];
			}
		}

		// advance
		srcdata += srcbytes;
		dstdata += dstbytes;
	}

	IDirect3DTexture9_UnlockRect (texture, level);
}


void D3D_CopyTextureLevel (LPDIRECT3DTEXTURE9 srctex, int srclevel, LPDIRECT3DTEXTURE9 dsttex, int dstlevel)
{
	LPDIRECT3DSURFACE9 srcsurf;
	LPDIRECT3DSURFACE9 dstsurf;

	IDirect3DTexture9_GetSurfaceLevel (srctex, srclevel, &srcsurf);
	IDirect3DTexture9_GetSurfaceLevel (dsttex, dstlevel, &dstsurf);

	D3DXLoadSurfaceFromSurface (dstsurf, NULL, NULL, srcsurf, NULL, NULL, D3DX_FILTER_POINT, 0);

	D3D_SAFE_RELEASE (srcsurf);
	D3D_SAFE_RELEASE (dstsurf);
}


void glGetTexImage (GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels)
{
	int i;
	int dstbytes = 0;
	unsigned char *srcdata;
	unsigned char *dstdata;
	LPDIRECT3DSURFACE9 texsurf;
	LPDIRECT3DSURFACE9 locksurf;
	D3DLOCKED_RECT lockrect;
	D3DSURFACE_DESC desc;

	if (target != GL_TEXTURE_2D) return;
	if (type != GL_UNSIGNED_BYTE) return;

	if (format == GL_RGB)
		dstbytes = 3;
	else if (format == GL_RGBA ||  format == GL_RGBA8 || format == GL_BGRA_EXT)
		dstbytes = 4;
	else Con_Printf ("glGetTexImage: invalid format");

	hr = IDirect3DTexture9_GetSurfaceLevel (d3d_TMUs[d3d_CurrentTMU].boundtexture->teximg, level, &texsurf);

	if (FAILED (hr) || !texsurf)
	{
		Con_Printf ("glGetTexImage: failed to access back buffer\n");
		return;
	}

	// because textures can be different formats we create it as one big enough to hold them all
	hr = IDirect3DSurface9_GetDesc (texsurf, &desc);
	//hr = IDirect3DDevice9_CreateImageSurface (d3d_Device, desc.Width, desc.Height, D3DFMT_A8R8G8B8, &locksurf);
	hr = D3DXLoadSurfaceFromSurface (locksurf, NULL, NULL, texsurf, NULL, NULL, D3DX_FILTER_NONE, 0);

	// now we have a surface we can lock
	hr = IDirect3DSurface9_LockRect (locksurf, &lockrect, NULL, D3DLOCK_READONLY);
	srcdata = (unsigned char *) lockrect.pBits;
	dstdata = (unsigned char *) pixels;

	for (i = 0; i < desc.Width * desc.Height; i++)
	{
		// swap back
		dstdata[0] = srcdata[2];
		dstdata[1] = srcdata[1];
		dstdata[2] = srcdata[0];

		if (dstbytes == 4) dstdata[3] = srcdata[3];

		srcdata += 4;
		dstdata += dstbytes;
	}

	// done
	hr = IDirect3DSurface9_UnlockRect (locksurf);
	D3D_SAFE_RELEASE (locksurf);
	D3D_SAFE_RELEASE (texsurf);
}


void glTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	D3DFORMAT texformat = D3DFMT_X8R8G8B8;

	// validate format
	switch (internalformat)
	{
	case 1:
	case GL_LUMINANCE:
		texformat = D3DFMT_L8;
		break;

	case 3:
	case GL_RGB:
		texformat = D3DFMT_X8R8G8B8;
		break;

	case 4:
	case GL_RGBA:
	case GL_RGBA8:
	case GL_BGRA_EXT:
		texformat = D3DFMT_A8R8G8B8;
		break;

	default:
		Sys_Error ("invalid texture internal format");
	}

	//if (type != GL_UNSIGNED_BYTE) Sys_Error ("glTexImage2D: Unrecognised pixel format");

	// ensure that it's valid to create textures
	//if (target != GL_TEXTURE_2D) return;
	if (!d3d_TMUs[d3d_CurrentTMU].boundtexture) return;

	if (level == 0)
	{
		// in THEORY an OpenGL texture can have different formats and internal formats for each miplevel
		// in practice I don't think anyone uses it...
		d3d_TMUs[d3d_CurrentTMU].boundtexture->internalformat = internalformat;

		// overwrite an existing texture - just release it so that we can recreate
		D3D_SAFE_RELEASE (d3d_TMUs[d3d_CurrentTMU].boundtexture->teximg);

		// create the texture from the data
		// initially just create a single mipmap level (assume that the texture isn't mipped)
		hr = IDirect3DDevice9_CreateTexture
		(
			d3d_Device,
			width,
			height,
			1,
			0,
			texformat,
			D3DPOOL_MANAGED,
			&d3d_TMUs[d3d_CurrentTMU].boundtexture->teximg,NULL
		);

		if (FAILED (hr)) Sys_Error ("glTexImage2D: unable to create a texture");

		D3D_FillTextureLevel (d3d_TMUs[d3d_CurrentTMU].boundtexture->teximg, 0, internalformat, width, height, format, pixels);
	}
	else if (level == 1)
	{
		// we're creating subsequent miplevels so we need to recreate this texture
		LPDIRECT3DTEXTURE9 newtexture;

		// this is miplevel 1 and we're recreating level 0, so double width and height
		hr = IDirect3DDevice9_CreateTexture
		(
			d3d_Device,
			width * 2,
			height * 2,
			0,
			0,
			texformat,
			D3DPOOL_MANAGED,
			&newtexture, NULL
		);

		if (FAILED (hr)) Sys_Error ("glTexImage2D: unable to create a texture");

		// copy level 0 across to the new texture
		D3D_CopyTextureLevel (d3d_TMUs[d3d_CurrentTMU].boundtexture->teximg, 0, newtexture, 0);

		// release the former level 0 texture and reset to the new one
		D3D_SAFE_RELEASE (d3d_TMUs[d3d_CurrentTMU].boundtexture->teximg);
		d3d_TMUs[d3d_CurrentTMU].boundtexture->teximg = newtexture;

		D3D_FillTextureLevel (d3d_TMUs[d3d_CurrentTMU].boundtexture->teximg, 1, internalformat, width, height, format, pixels);
	}
	else
	{
		// the texture has already been created so no need to do any more
		D3D_FillTextureLevel (d3d_TMUs[d3d_CurrentTMU].boundtexture->teximg, level, internalformat, width, height, format, pixels);
	}
}


void glGetTexParameterfv (GLenum target, GLenum pname, GLfloat *params)
{
	if (!d3d_TMUs[d3d_CurrentTMU].boundtexture) return;
	if (target != GL_TEXTURE_2D) return;

	switch (pname)
	{
	case GLD3D_TEXTURE_MAX_ANISOTROPY_EXT:
		params[0] = d3d_TMUs[d3d_CurrentTMU].boundtexture->anisotropy;
		break;

	default:
		break;
	}
}

void glTexParameterf (GLenum target, GLenum pname, GLfloat param)
{
	if (!d3d_TMUs[d3d_CurrentTMU].boundtexture) return;
	if (target != GL_TEXTURE_2D) return;

	d3d_TMUs[d3d_CurrentTMU].texparamdirty = TRUE;

	switch (pname)
	{
	case GL_TEXTURE_MIN_FILTER:
		if ((int) param == GL_NEAREST_MIPMAP_NEAREST)
		{
			d3d_TMUs[d3d_CurrentTMU].boundtexture->minfilter = D3DTEXF_POINT;
			d3d_TMUs[d3d_CurrentTMU].boundtexture->mipfilter = D3DTEXF_POINT;
		}
		else if ((int) param == GL_LINEAR_MIPMAP_NEAREST)
		{
			d3d_TMUs[d3d_CurrentTMU].boundtexture->minfilter = D3DTEXF_LINEAR;
			d3d_TMUs[d3d_CurrentTMU].boundtexture->mipfilter = D3DTEXF_POINT;
		}
		else if ((int) param == GL_NEAREST_MIPMAP_LINEAR)
		{
			d3d_TMUs[d3d_CurrentTMU].boundtexture->minfilter = D3DTEXF_POINT;
			d3d_TMUs[d3d_CurrentTMU].boundtexture->mipfilter = D3DTEXF_LINEAR;
		}
		else if ((int) param == GL_LINEAR_MIPMAP_LINEAR)
		{
			d3d_TMUs[d3d_CurrentTMU].boundtexture->minfilter = D3DTEXF_LINEAR;
			d3d_TMUs[d3d_CurrentTMU].boundtexture->mipfilter = D3DTEXF_LINEAR;
		}
		else if ((int) param == GL_LINEAR)
		{
			d3d_TMUs[d3d_CurrentTMU].boundtexture->minfilter = D3DTEXF_LINEAR;
			d3d_TMUs[d3d_CurrentTMU].boundtexture->mipfilter = D3DTEXF_NONE;
		}
		else
		{
			// GL_NEAREST
			d3d_TMUs[d3d_CurrentTMU].boundtexture->minfilter = D3DTEXF_POINT;
			d3d_TMUs[d3d_CurrentTMU].boundtexture->mipfilter = D3DTEXF_NONE;
		}
		break;

	case GL_TEXTURE_MAG_FILTER:
		if ((int) param == GL_LINEAR)
			d3d_TMUs[d3d_CurrentTMU].boundtexture->magfilter = D3DTEXF_LINEAR;
		else d3d_TMUs[d3d_CurrentTMU].boundtexture->magfilter = D3DTEXF_POINT;
		break;

	case GL_TEXTURE_WRAP_S:
		if ((int) param == GL_CLAMP)
			d3d_TMUs[d3d_CurrentTMU].boundtexture->addressu = D3DTADDRESS_CLAMP;
		else d3d_TMUs[d3d_CurrentTMU].boundtexture->addressu = D3DTADDRESS_WRAP;
		break;

	case GL_TEXTURE_WRAP_T:
		if ((int) param == GL_CLAMP)
			d3d_TMUs[d3d_CurrentTMU].boundtexture->addressv = D3DTADDRESS_CLAMP;
		else d3d_TMUs[d3d_CurrentTMU].boundtexture->addressv = D3DTADDRESS_WRAP;
		break;

	case GLD3D_TEXTURE_MAX_ANISOTROPY_EXT:
		// this is a texparam in OpenGL
		if (d3d_Caps.MaxAnisotropy > 1)
			d3d_TMUs[d3d_CurrentTMU].boundtexture->anisotropy = (int) param;
		else d3d_TMUs[d3d_CurrentTMU].boundtexture->anisotropy = 1;
		break;

	default:
		break;
	}
}


void glTexParameteri (GLenum target, GLenum pname, GLint param)
{
	if (target != GL_TEXTURE_2D) return;
	if (!d3d_TMUs[d3d_CurrentTMU].boundtexture) return;

	glTexParameterf (target, pname, param);
}


void glTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
	int x, y;
	int srcbytes = 0;
	int dstbytes = 0;
	byte *srcdata;
	byte *dstdata;
	D3DLOCKED_RECT lockrect;

	if (format == 1 || format == GL_LUMINANCE)
		srcbytes = 1;
	else if (format == 3 || format == GL_RGB)
		srcbytes = 3;
	else if (format == 4 || format == GL_RGBA || format == GL_RGBA8 || format ==  GL_BGRA_EXT)
		srcbytes = 4;
	else Sys_Error ("D3D_FillTextureLevel: illegal format");

	// d3d doesn't have an internal RGB only format
	// (neither do most OpenGL implementations, they just let you specify it as RGB but expand internally to 4 component)
	if (d3d_TMUs[d3d_CurrentTMU].boundtexture->internalformat == 1 || d3d_TMUs[d3d_CurrentTMU].boundtexture->internalformat == GL_LUMINANCE)
		dstbytes = 1;
	else if (d3d_TMUs[d3d_CurrentTMU].boundtexture->internalformat == 3 || d3d_TMUs[d3d_CurrentTMU].boundtexture->internalformat == GL_RGB)
		dstbytes = 4;
	else if (d3d_TMUs[d3d_CurrentTMU].boundtexture->internalformat == 4 || d3d_TMUs[d3d_CurrentTMU].boundtexture->internalformat == GL_RGBA || d3d_TMUs[d3d_CurrentTMU].boundtexture->internalformat == GL_RGBA8 || d3d_TMUs[d3d_CurrentTMU].boundtexture->internalformat == GL_BGRA_EXT)
		dstbytes = 4;
	else Sys_Error ("D3D_FillTextureLevel: illegal internalformat");

	IDirect3DTexture9_LockRect (d3d_TMUs[d3d_CurrentTMU].boundtexture->teximg, level, &lockrect, NULL, 0);

	srcdata = (byte *) pixels;
	dstdata = lockrect.pBits;
	dstdata += (yoffset * width + xoffset) * dstbytes;

	for (y = yoffset; y < (yoffset + height); y++)
	{
		for (x = xoffset; x < (xoffset + width); x++)
		{
			if (srcbytes == 1)
			{
				if (dstbytes == 1)
					dstdata[0] = srcdata[0];
				else if (dstbytes == 4)
				{
					dstdata[0] = srcdata[0];
					dstdata[1] = srcdata[0];
					dstdata[2] = srcdata[0];
					dstdata[3] = srcdata[0];
				}
			}
			else if (srcbytes == 3)
			{
				if (dstbytes == 1)
					dstdata[0] = ((int) srcdata[0] + (int) srcdata[1] + (int) srcdata[2]) / 3;
				else if (dstbytes == 4)
				{
					dstdata[0] = srcdata[2];
					dstdata[1] = srcdata[1];
					dstdata[2] = srcdata[0];
					dstdata[3] = 255;
				}
			}
			else if (srcbytes == 4)
			{
				if (dstbytes == 1)
					dstdata[0] = ((int) srcdata[0] + (int) srcdata[1] + (int) srcdata[2]) / 3;
				else if (dstbytes == 4)
				{
					dstdata[0] = srcdata[2];
					dstdata[1] = srcdata[1];
					dstdata[2] = srcdata[0];
					dstdata[3] = srcdata[3];
				}
			}

			// advance
			srcdata += srcbytes;
			dstdata += dstbytes;
		}
	}

	IDirect3DTexture9_UnlockRect (d3d_TMUs[d3d_CurrentTMU].boundtexture->teximg, level);
}


void glBindTexture (GLenum target, GLuint texture)
{
	d3d_texture_t *tex;

	//if (target != GL_TEXTURE_2D) return;

	// use no texture
	if (texture == 0)
	{
		d3d_TMUs[d3d_CurrentTMU].boundtexture = NULL;
		return;
	}

	// initially nothing
	d3d_TMUs[d3d_CurrentTMU].boundtexture = NULL;

	// find a texture
	// these searches could be optimised with another lookup table, but we don't know how big it would need to be
	for (tex = d3d_Textures; tex; tex = tex->next)
	{
		if (tex->glnum == texture)
		{
			d3d_TMUs[d3d_CurrentTMU].boundtexture = tex;
			break;
		}
	}

	// did we find it?
	if (!d3d_TMUs[d3d_CurrentTMU].boundtexture)
	{
		// nope, so fill in a new one (this will make it work with texture_extension_number)
		// (i don't know if the spec formally allows this but id seem to have gotten away with it...)
		d3d_TMUs[d3d_CurrentTMU].boundtexture = D3D_AllocTexture ();

		// reserve this slot
		d3d_TMUs[d3d_CurrentTMU].boundtexture->glnum = texture;

		// ensure that it won't be reused
		if (texture > d3d_TextureExtensionNumber) d3d_TextureExtensionNumber = texture;
	}

	// this should never happen
	if (!d3d_TMUs[d3d_CurrentTMU].boundtexture) Sys_Error ("glBindTexture: out of textures!!!");

	// dirty the params
	d3d_TMUs[d3d_CurrentTMU].texparamdirty = TRUE;
}


void glGenTextures (GLsizei n, GLuint *textures)
{
	int i;

	for (i = 0; i < n; i++)
	{
		// either take a free slot or alloc a new one
		d3d_texture_t *tex = D3D_AllocTexture ();

		tex->glnum = textures[i] = d3d_TextureExtensionNumber;
		d3d_TextureExtensionNumber++;
	}
}


void glDeleteTextures (GLsizei n, const GLuint *textures)
{
	int i;
	d3d_texture_t *tex;

	for (tex = d3d_Textures; tex; tex = tex->next)
	{
		for (i = 0; i < n; i++)
		{
			if (tex->glnum == textures[i])
			{
				D3D_InitTexture (tex);
				break;
			}
		}
	}
}


/*
===================================================================================================================

			VIDEO SETUP

===================================================================================================================
*/

/*
==================
D3D_GetDepthFormat

Gets a valid depth format for a given adapter format
==================
*/
D3DFORMAT D3D_GetDepthFormat (D3DFORMAT AdapterFormat)
{
	// valid depth formats
	int i;
	D3DFORMAT d3d_DepthFormats[] = {D3DFMT_D32, D3DFMT_D24X8, D3DFMT_D24S8,  D3DFMT_D16, D3DFMT_UNKNOWN};

	if (d3d_RequestStencil)
	{
		// switch to formats with a stencil buffer available at the head of the list
		d3d_DepthFormats[0] = D3DFMT_D24S8;		 
		d3d_DepthFormats[1] = D3DFMT_D32;
		d3d_DepthFormats[2] = D3DFMT_D24X8;
		d3d_DepthFormats[3] = D3DFMT_D16;
		d3d_DepthFormats[4] = D3DFMT_UNKNOWN;
	}

	for (i = 0; ; i++)
	{
		if (d3d_DepthFormats[i] == D3DFMT_UNKNOWN)
			break;

		hr = IDirect3D9_CheckDeviceFormat
		(
			d3d_Object,
			D3DADAPTER_DEFAULT,
			D3DDEVTYPE_HAL,
			AdapterFormat,
			D3DUSAGE_DEPTHSTENCIL,
			D3DRTYPE_SURFACE,
			d3d_DepthFormats[i]
		);

		// return the first format that succeeds
		if (SUCCEEDED (hr)) return d3d_DepthFormats[i];
	}

	// didn't get one
	return D3DFMT_UNKNOWN;
}


/*
========================
D3D_GetAdapterModeFormat

returns a usable adapter mode for the given width, height and bpp
========================
*/
D3DFORMAT D3D_GetAdapterModeFormat (int width, int height, int bpp)
{
	int i;

	// fill these in depending on bpp
	D3DFORMAT d3d_Formats[4];

	// these are the orders in which we prefer our formats
	if (bpp == -1)
	{
		// unspecified bpp uses the desktop mode format
		d3d_Formats[0] = d3d_DesktopMode.Format;
		d3d_Formats[1] = D3DFMT_UNKNOWN;
	}
	else if (bpp == 16)
	{
		d3d_Formats[0] = D3DFMT_R5G6B5;
		d3d_Formats[1] = D3DFMT_X1R5G5B5;
		d3d_Formats[2] = D3DFMT_A1R5G5B5;
		d3d_Formats[3] = D3DFMT_UNKNOWN;
	}
	else
	{
		d3d_Formats[0] = D3DFMT_X8R8G8B8;
		d3d_Formats[1] = D3DFMT_A8R8G8B8;
		d3d_Formats[2] = D3DFMT_UNKNOWN;
	}
 

 
	return d3d_Formats[0];
}


BOOL WINAPI SetPixelFormat (HDC hdc, int format, CONST PIXELFORMATDESCRIPTOR *ppfd)
{
	if (ppfd->cStencilBits)
		d3d_RequestStencil = TRUE;
	else
		d3d_RequestStencil = FALSE;

	// just silently pass the PFD through unmodified
	return TRUE;
}


void D3D_SetupPresentParams (int width, int height, int bpp, BOOL windowed)
{
	// clear present params to NULL
	memset (&d3d_PresentParams, 0, sizeof (D3DPRESENT_PARAMETERS));

	// popup windows are fullscreen always
	if (windowed)
	{
		// defaults for windowed mode - also need to store out clientrect.right and clientrect.bottom
		// (d3d_BPP is only used for fullscreen modes and is retrieved from our CDS override)
		d3d_CurrentMode.Format = d3d_DesktopMode.Format;
		d3d_CurrentMode.Width = width;
		d3d_CurrentMode.Height = height;
		d3d_CurrentMode.RefreshRate = 0;
	}
	else
	{
		// also fills in d3d_CurrentMode
		D3DFORMAT fmt = D3D_GetAdapterModeFormat (width, height, d3d_BPP);

		// ensure that we got a good format
		if (fmt == D3DFMT_UNKNOWN)
			Sys_Error ("failed to get fullscreen mode");
	}

	// fill in mode-dependent stuff
	d3d_PresentParams.BackBufferFormat = D3DFMT_X8R8G8B8;
	d3d_PresentParams.FullScreen_RefreshRateInHz = d3d_CurrentMode.RefreshRate;
	d3d_PresentParams.Windowed = windowed;

	// request 1 backbuffer
	d3d_PresentParams.BackBufferCount = 1;
	d3d_PresentParams.BackBufferWidth = width;
	d3d_PresentParams.BackBufferHeight = height;

	d3d_PresentParams.EnableAutoDepthStencil = TRUE;
	d3d_PresentParams.AutoDepthStencilFormat = D3DFMT_D24S8;
	d3d_PresentParams.MultiSampleType = D3DMULTISAMPLE_NONE;
	d3d_PresentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3d_PresentParams.hDeviceWindow = d3d_Window;
}


BOOL WINAPI wglMakeCurrent(HDC hdc, HGLRC hglrc)
{
	ID3DXBuffer* pShaderCode;
	RECT clientrect;
	LONG winstyle;

	XMMATRIX matWorld = XMMatrixIdentity();
    // View matrix
    XMVECTOR vEyePt = XMVectorSet( 0.0f, -4.0f, -4.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    XMMATRIX matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUp );

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )1280 / ( FLOAT )720;

    // Projection matrix
    XMMATRIX matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspectRatio, 1.0f, 200.0f );

    // World*view*projection
    XMMATRIX matWVP = XMMatrixMultiply(matProj, matView); 
	matWVP = XMMatrixMultiply(matWorld, matWVP); 
 
	if (hdc)
	{
		d3d_Device = hdc;
		 
	}

	if (hglrc)
	{
		d3d_Object = hglrc;
	}

	// if there is no D3D Object we must return FALSE
	// d3d_Object is created in wglCreateContext so this behaviour will enforce wglCreateContext to be called before wglMakeCurrent
	// note that this will break in sitauions where there is more than one active context and we're switching between them...
	//if (!d3d_Object) return FALSE;


	// if a device was previously set up, force a subsequent request to MakeCurrent to fail
	//if (d3d_Device) return FALSE;


	if (FAILED (hr = IDirect3D9_GetDeviceCaps (d3d_Object, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &d3d_Caps)))
		Sys_Error ("failed to get object caps");

 
	// setup our present parameters (popup windows are fullscreen always)
	D3D_SetupPresentParams (1280, 720, d3d_BPP, FALSE);

	// here we use D3DCREATE_FPU_PRESERVE to maintain the resolution of Quake's timers (this is a serious problem)
	// and D3DCREATE_DISABLE_DRIVER_MANAGEMENT to protect us from rogue drivers (call it honest paranoia).  first
	// we attempt to create a hardware vp device.
	// --------------------------------------------------------------------------------------------------------
	// NOTE re pure devices: we intentionally DON'T request a pure device, EVEN if one is available, as we need
	// to support certain glGet functions that would be broken if we used one.
	// --------------------------------------------------------------------------------------------------------
	// NOTE re D3DCREATE_FPU_PRESERVE - this can be avoided if we use a timer that's not subject to FPU drift,
	// such as timeGetTime (with timeBeginTime (1)); by default Quake's times *ARE* prone to FPU drift as they
	// use doubles for storing the last time, which gradually creeps up to be nearer to the current time each
	// frame.  Not using doubles for the stored times (i.e. switching them all to floats) would also help here.
	
    if (!d3d_Device)
	{
	hr = IDirect3D9_CreateDevice
	(
		d3d_Object,
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		d3d_Window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE ,
		&d3d_PresentParams,
		&d3d_Device
	);

	if (FAILED (hr))
	{
		// it's OK, we may not have hardware vp available, so create a software vp device
		hr = IDirect3D9_CreateDevice
		(
			d3d_Object,
			D3DADAPTER_DEFAULT,
			D3DDEVTYPE_HAL,
			d3d_Window,
			 D3DCREATE_FPU_PRESERVE    ,
			&d3d_PresentParams,
			&d3d_Device
		);

		// oh shit
		if (hr == D3DERR_INVALIDCALL)
			Sys_Error ("IDirect3D9_CreateDevice with D3DERR_INVALIDCALL");
		else if (hr == D3DERR_NOTAVAILABLE)
			Sys_Error ("IDirect3D9_CreateDevice with D3DERR_NOTAVAILABLE");
		else if (hr == D3DERR_OUTOFVIDEOMEMORY)
			Sys_Error ("IDirect3D9_CreateDevice with D3DERR_INVALIDCALL");
		else if (FAILED (hr)) Sys_Error ("failed to create Direct3D device");
	}
	}
	if (d3d_Device == NULL) Sys_Error ("created NULL Direct3D device");

	// this also initializes the texture parameters for us
	D3D_ReleaseTextures ();

	// initialize state
	D3D_InitStates ();

	// disable lighting
	//D3D_SetRenderState (D3DRS_LIGHTING, FALSE);

	// default zbias
	// D3D uses values of 0 to 16, with 0 being the default and higher values coming nearer
	// because we want to push out further too, we pick an intermediate value as our new default
	//D3D_SetRenderState (D3DRS_ZBIAS, 8);

	// set projection and world to dirty, beginning of stack and identity
	D3D_InitializeMatrix (&d3d_ModelViewMatrix);
	D3D_InitializeMatrix (&d3d_ProjectionMatrix);




	


	D3DXCompileShader( g_strPixelShader,
                            ( UINT )strlen( g_strPixelShader ),
                            NULL,
                            NULL,
                            "main",
                            "ps_2_0",
                            0,
                            &pPixelShaderCode,
                            &pPixelErrorMsg,
                            NULL );

	g_pPixelShader = D3DDevice_CreatePixelShader( ( DWORD* )pPixelShaderCode->lpVtbl->GetBufferPointer(pPixelShaderCode));
 
    
    if( FAILED( D3DXCompileShader( g_strVertexShader, strlen( g_strVertexShader ),
                                       NULL, NULL, "VShader", "vs_2_0", 0,
                                       &pShaderCode, NULL, NULL ) ) )
           return 0;
		 
    IDirect3DDevice9_CreateVertexShader(d3d_Device,( DWORD* )pShaderCode->lpVtbl->GetBufferPointer(pShaderCode), &g_pGradientVertexShader);
	IDirect3DDevice9_CreateVertexDeclaration(d3d_Device,declOGL,&vertexDeclaration);
	IDirect3DDevice9_CreateVertexBuffer(d3d_Device,64*sizeof(gl_vertex_t), D3DUSAGE_CPU_CACHED_MEMORY,0L, D3DPOOL_DEFAULT, &vertexBuffer, NULL );

	D3DDevice_SetVertexShader(d3d_Device, g_pGradientVertexShader );
	D3DDevice_SetPixelShader(d3d_Device, g_pPixelShader);
	D3DDevice_SetVertexDeclaration(d3d_Device, vertexDeclaration);

	D3DDevice_SetVertexShaderConstantF(d3d_Device,  0, ( FLOAT* )&matWVP, 4 );
	
	return TRUE;
}


HGLRC WINAPI wglCreateContext (HDC hdc)
{
	// initialize direct3d and get object capabilities
	if (!(d3d_Object = Direct3DCreate9 (D3D_SDK_VERSION)))
		Sys_Error ("Failed to create Direct3D Object");

	return (HGLRC) d3d_Object;
}


BOOL WINAPI wglDeleteContext (HGLRC hglrc)
{
	// release all textures
	D3D_ReleaseTextures ();

	// release device and object
	D3D_SAFE_RELEASE (d3d_Device);
	D3D_SAFE_RELEASE (d3d_Object);

	// success
	return TRUE;
}


HGLRC WINAPI wglGetCurrentContext (VOID)
{
	return (HGLRC) 1;
}


HDC WINAPI wglGetCurrentDC (VOID)
{
	return 0;
}


/*
===================================================================================================================

			COMPARISON TEST AND FUNCTION

	Just DepthFunc and AlphaFunc really

===================================================================================================================
*/

void GL_SetCompFunc (DWORD mode, GLenum func)
{
	switch (func)
	{
	case GL_NEVER:
		D3D_SetRenderState (mode, D3DCMP_NEVER);
		break;

	case GL_LESS:
		D3D_SetRenderState (mode, D3DCMP_LESS);
		break;

	case GL_LEQUAL:
		D3D_SetRenderState (mode, D3DCMP_LESSEQUAL);
		break;

	case GL_EQUAL:
		D3D_SetRenderState (mode, D3DCMP_EQUAL);
		break;

	case GL_GREATER:
		D3D_SetRenderState (mode, D3DCMP_GREATER);
		break;

	case GL_NOTEQUAL:
		D3D_SetRenderState (mode, D3DCMP_NOTEQUAL);
		break;

	case GL_GEQUAL:
		D3D_SetRenderState (mode, D3DCMP_GREATEREQUAL);
		break;

	case GL_ALWAYS:
	default:
		D3D_SetRenderState (mode, D3DCMP_ALWAYS);
		break;
	}
}


void glDepthFunc (GLenum func)
{
	GL_SetCompFunc (D3DRS_ZFUNC, func);
}


void glAlphaFunc (GLenum func, GLclampf ref)
{
	GL_SetCompFunc (D3DRS_ALPHAFUNC, func);
	D3D_SetRenderState (D3DRS_ALPHAREF, BYTE_CLAMP ((int) (ref * 255)));
}


/*
===================================================================================================================

			FRAMEBUFFER

===================================================================================================================
*/


void glColorMask (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	DWORD mask = 0;

	if (red) mask |= D3DCOLORWRITEENABLE_RED;
	if (green) mask |= D3DCOLORWRITEENABLE_GREEN;
	if (blue) mask |= D3DCOLORWRITEENABLE_BLUE;
	if (alpha) mask |= D3DCOLORWRITEENABLE_ALPHA;

	D3D_SetRenderState (D3DRS_COLORWRITEENABLE, mask);
}


void glDepthRange (GLclampd zNear, GLclampd zFar)
{
	// update the viewport
	d3d_Viewport.MinZ = zNear;
	d3d_Viewport.MaxZ = zFar;

	IDirect3DDevice9_SetViewport (d3d_Device, &d3d_Viewport);
	D3D_DirtyAllStates ();
}


void glDepthMask (GLboolean flag)
{
	// if only they were all so easy...
	D3D_SetRenderState (D3DRS_ZWRITEENABLE, flag == GL_TRUE ? TRUE : FALSE);
}


void GL_Blend (D3DRENDERSTATETYPE rs, GLenum factor)
{
	D3DBLEND blend = D3DBLEND_ONE;

	switch (factor)
	{
	case GL_ZERO:
		blend = D3DBLEND_ZERO;
		break;

	case GL_ONE:
		blend = D3DBLEND_ONE;
		break;

	case GL_SRC_COLOR:
		blend = D3DBLEND_SRCCOLOR;
		break;

	case GL_ONE_MINUS_SRC_COLOR:
		blend = D3DBLEND_INVSRCCOLOR;
		break;

	case GL_DST_COLOR:
		blend = D3DBLEND_DESTCOLOR;
		break;

	case GL_ONE_MINUS_DST_COLOR:
		blend = D3DBLEND_INVDESTCOLOR;
		break;

	case GL_SRC_ALPHA:
		blend = D3DBLEND_SRCALPHA;
		break;

	case GL_ONE_MINUS_SRC_ALPHA:
		blend = D3DBLEND_INVSRCALPHA;
		break;

	case GL_DST_ALPHA:
		blend = D3DBLEND_DESTALPHA;
		break;

	case GL_ONE_MINUS_DST_ALPHA:
		blend = D3DBLEND_INVDESTALPHA;
		break;

	case GL_SRC_ALPHA_SATURATE:
		blend = D3DBLEND_SRCALPHASAT;
		break;

	default:
		Sys_Error ("glBlendFunc: unknown factor");
	}

	D3D_SetRenderState (rs, blend);
}


void glBlendFunc (GLenum sfactor, GLenum dfactor)
{
	GL_Blend (D3DRS_SRCBLEND, sfactor);
	GL_Blend (D3DRS_DESTBLEND, dfactor);
}


void glClear (GLbitfield mask)
{
	DWORD clearflags = 0;

	// no accumulation buffer in d3d
	if (mask & GL_COLOR_BUFFER_BIT) clearflags |= D3DCLEAR_TARGET;
	if (mask & GL_DEPTH_BUFFER_BIT) clearflags |= D3DCLEAR_ZBUFFER;
	if (mask & GL_STENCIL_BUFFER_BIT) clearflags |= D3DCLEAR_STENCIL;

	// back to normal operation
	IDirect3DDevice9_Clear (d3d_Device, 0, NULL, clearflags, d3d_ClearColor, 1, 1);
}


void glClearColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	d3d_ClearColor = GL_ColorToD3D (red, green, blue, alpha);
}


void glDrawBuffer (GLenum mode)
{
	// d3d doesn't like us messing with the front buffer, which is all glquake uses this for, so here
	// we just silently ignore requests to change the draw buffer
}


void glFinish (void)
{
	// we force a Present in our SwapBuffers function so this is unneeded
}


void glReadBuffer (GLenum mode)
{
	// d3d doesn't like us messing with the front buffer, which is all glquake uses this for, so here
	// we just silently ignore requests to change the draw buffer
}


void glReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels)
{
	int row;
	int col;
	unsigned char *srcdata;
	unsigned char *dstdata = (unsigned char *) pixels;
	LPDIRECT3DSURFACE9 bbsurf;
	LPDIRECT3DSURFACE9 locksurf;
	D3DLOCKED_RECT lockrect;
	D3DSURFACE_DESC desc;

	if (format != GL_RGB || type != GL_UNSIGNED_BYTE)
	{
		Con_Printf ("glReadPixels: invalid format or type\n");
		return;
	}

	hr = IDirect3DDevice9_GetBackBuffer (d3d_Device, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bbsurf);

	if (FAILED (hr) || !bbsurf)
	{
		Con_Printf ("glReadPixels: failed to access back buffer\n");
		return;
	}

	// because we don't have a lockable backbuffer we instead copy it off to an image surface
	// this will also handle translation between different backbuffer formats
	hr = IDirect3DSurface9_GetDesc (bbsurf, &desc);
	//hr = IDirect3DDevice9_CreateImageSurface (d3d_Device, desc.Width, desc.Height, D3DFMT_A8R8G8B8, &locksurf);
	hr = D3DXLoadSurfaceFromSurface (locksurf, NULL, NULL, bbsurf, NULL, NULL, D3DX_FILTER_NONE, 0);

	// now we have a surface we can lock
	hr = IDirect3DSurface9_LockRect (locksurf, &lockrect, NULL, D3DLOCK_READONLY);
	srcdata = (unsigned char *) lockrect.pBits;

	// invert the copied image
	for (row = (y + height - 1); row >= y; row--)
	{
		for (col = x; col < (x + width); col++)
		{
			int srcpos = row * width + col;

			// swap to BGR because Quake is going to swap it back
			dstdata[2] = srcdata[srcpos * 3 + 0];
			dstdata[1] = srcdata[srcpos * 3 + 1];
			dstdata[0] = srcdata[srcpos * 3 + 2];

			dstdata += 3;
		}
	}

	hr = IDirect3DSurface9_UnlockRect (locksurf);	 
	D3D_SAFE_RELEASE (locksurf);
	D3D_SAFE_RELEASE (bbsurf);
}


void glViewport (GLint x, GLint y, GLsizei width, GLsizei height)
{
	// translate from OpenGL bottom-left to D3D top-left
	y = d3d_CurrentMode.Height - (height + y);

	// always take the full depth range
	d3d_Viewport.X = x;
	d3d_Viewport.Y = y;
	d3d_Viewport.Width = width;
	d3d_Viewport.Height = height;
	d3d_Viewport.MinZ = 0;
	d3d_Viewport.MaxZ = 1;

	IDirect3DDevice9_SetViewport (d3d_Device, &d3d_Viewport);

	// dirty all states
	D3D_DirtyAllStates ();
}


void D3D_PostResetRestore (void)
{
	// set projection and world to dirty, beginning of stack and identity
	D3D_InitializeMatrix (&d3d_ModelViewMatrix);
	D3D_InitializeMatrix (&d3d_ProjectionMatrix);

	// force all states back to the way they were
	D3D_SetRenderStates ();
	D3D_SetTextureStates ();

	// set viewport to invalid
	d3d_Viewport.Width = -1;
	d3d_Viewport.Height = -1;
}


void D3D_ResetMode (int width, int height, int bpp, BOOL windowed)
{
 

	// reset present params
	D3D_SetupPresentParams (width, height, bpp, windowed);

	// reset device
	IDirect3DDevice9_Reset (d3d_Device, &d3d_PresentParams);

 
	// clean up states/etc
	D3D_PostResetRestore ();

	// because a lot of things are now bouncing around the system we take a breather
	Sleep (500);

 
}


void FakeSwapBuffers (void)
{
	// if we lost the device (e.g. on a mode switch, alt-tab, etc) we must try to recover it
	if (d3d_DeviceLost)
	{
		// here we get the current status of the device
		hr = D3D_OK;

		switch (hr)
		{
		case D3D_OK:
			// device is recovered
			d3d_DeviceLost = FALSE;
			D3D_PostResetRestore ();
			break;

		case D3DERR_DEVICELOST:
			// device is still lost
			break;

		case D3DERR_DEVICENOTRESET:
			// device is ready to be reset
			IDirect3DDevice9_Reset (d3d_Device, &d3d_PresentParams);
			break;

		default:
			break;
		}

		// yield the CPU a little
		Sleep (10);

		// don't bother this frame
		return;
	}

	if (d3d_SceneBegun)
	{
		// see do we need to reset the viewport as D3D restricts present to the client viewport
		// this is needed as some engines (hello FitzQuake) set multiple smaller viewports for
		// different on-screen elements (e.g. menus)
		BOOL vpreset = FALSE;

		// test dimensions for a change; we don't just blindly reset it as this may not happen every frame
		if (d3d_Viewport.X != 0) vpreset = TRUE;
		if (d3d_Viewport.Y != 0) vpreset = TRUE;
		if (d3d_Viewport.Width != d3d_CurrentMode.Width) vpreset = TRUE;
		if (d3d_Viewport.Height != d3d_CurrentMode.Height) vpreset = TRUE;

		if (vpreset)
		{
			// now reset it to full window dimensions so that the full window will present
			d3d_Viewport.X = 0;
			d3d_Viewport.Y = 0;
			d3d_Viewport.Width = d3d_CurrentMode.Width;
			d3d_Viewport.Height = d3d_CurrentMode.Height;
			IDirect3DDevice9_SetViewport (d3d_Device, &d3d_Viewport);
		}

		// endscene and present are only required if a scene was begun (i.e. if something was actually drawn)
		IDirect3DDevice9_EndScene (d3d_Device);
		d3d_SceneBegun = FALSE;

		// present the display
		hr = IDirect3DDevice9_Present (d3d_Device, NULL, NULL, NULL, NULL);

		if (hr == D3DERR_DEVICELOST)
		{
			// flag a lost device
			d3d_DeviceLost = TRUE;
		}
		else if (FAILED (hr))
		{
			// something else bad happened
			Sys_Error ("FAILED (hr) on IDirect3DDevice9_Present");
		}
	}
}


/*
===================================================================================================================

			OTHER

===================================================================================================================
*/

void GL_UpdateCull (void)
{
	if (gl_CullEnable == GL_FALSE)
	{
		// disable culling
		D3D_SetRenderState (D3DRS_CULLMODE, D3DCULL_NONE);
		return;
	}

	if (gl_FrontFace == GL_CCW)
	{
		if (gl_CullMode == GL_BACK)
			D3D_SetRenderState (D3DRS_CULLMODE, D3DCULL_CW);
		else if (gl_CullMode == GL_FRONT)
			D3D_SetRenderState (D3DRS_CULLMODE, D3DCULL_CCW);
		else if (gl_CullMode == GL_FRONT_AND_BACK)
			; // do nothing; we cull in software in GL_SubmitVertexes instead
		else Sys_Error ("GL_UpdateCull: illegal glCullFace");
	}
	else if (gl_FrontFace == GL_CW)
	{
		if (gl_CullMode == GL_BACK)
			D3D_SetRenderState (D3DRS_CULLMODE, D3DCULL_CCW);
		else if (gl_CullMode == GL_FRONT)
			D3D_SetRenderState (D3DRS_CULLMODE, D3DCULL_CW);
		else if (gl_CullMode == GL_FRONT_AND_BACK)
			; // do nothing; we cull in software in GL_SubmitVertexes instead
		else Sys_Error ("GL_UpdateCull: illegal glCullFace");
	}
	else Sys_Error ("GL_UpdateCull: illegal glFrontFace");
}


void glFrontFace (GLenum mode)
{
	gl_FrontFace = mode;
	GL_UpdateCull ();
}


void glCullFace (GLenum mode)
{
	gl_CullMode = mode;
	GL_UpdateCull ();
}


void glScissor (GLint x, GLint y, GLsizei width, GLsizei height)
{
	// d3d8 doesnt have scissor test so we'll just live without it for now.
}


void GL_EnableDisable (GLenum cap, BOOL enabled)
{
	DWORD d3d_RS;

	switch (cap)
	{
	case GL_SCISSOR_TEST:
		// d3d8 doesnt have scissor test so we'll just live without it for now.
		break;

	case GL_BLEND:
		d3d_RS = D3DRS_ALPHABLENDENABLE;
		break;

	case GL_ALPHA_TEST:
		d3d_RS = D3DRS_ALPHATESTENABLE;
		break;

	case GL_TEXTURE_2D:
    case GLD3D_TEXTURE_RECTANGLE_ARB:
		// this is a texture stage state rather than a render state
		if (enabled)
			d3d_TMUs[d3d_CurrentTMU].enabled = TRUE;
		else d3d_TMUs[d3d_CurrentTMU].enabled = FALSE;

		d3d_TMUs[d3d_CurrentTMU].texenvdirty = TRUE;
		d3d_TMUs[d3d_CurrentTMU].texparamdirty = TRUE;

		// we're not setting state yet here...
		return;

	case GL_CULL_FACE:
		gl_CullEnable = enabled ? GL_TRUE : GL_FALSE;
		GL_UpdateCull ();

		// we're not setting state yet here...
		return;

	case GL_FOG:
		//d3d_RS = D3DRS_FOGENABLE;
		break;

	case GL_DEPTH_TEST:
		d3d_RS = D3DRS_ZENABLE;
		if (enabled) enabled = D3DZB_TRUE; else enabled = D3DZB_FALSE;
		break;

	case GL_POLYGON_OFFSET_FILL:
	case GL_POLYGON_OFFSET_LINE:
		d3d_PolyOffsetEnabled = enabled;
		d3d_PolyOffsetSwitched = FALSE;

		// we're not setting state yet here...
		return;

	default:
		return;
	}

	D3D_SetRenderState (d3d_RS, enabled);
}


void glDisable (GLenum cap)
{
	GL_EnableDisable (cap, FALSE);
}


void glEnable (GLenum cap)
{
	GL_EnableDisable (cap, TRUE);
}


void glGetFloatv (GLenum pname, GLfloat *params)
{
	switch (pname)
	{
	case GL_MODELVIEW_MATRIX:
		memcpy (params, d3d_ModelViewMatrix.stack[d3d_ModelViewMatrix.stackdepth].m, sizeof (float) * 16);
		break;

	case GLD3D_MAX_TEXTURE_MAX_ANISOTROPY_EXT:
		params[0] = (float) d3d_Caps.MaxAnisotropy;
		break;

	default:
		break;
	}
}


void glPolygonMode (GLenum face, GLenum mode)
{
	// we don't have the ability to specify which side of the poly is filled, so just do it
	// the way that it's specified and hope for the best!
	if (mode == GL_LINE)
		D3D_SetRenderState (D3DRS_FILLMODE, D3DFILL_WIREFRAME);
	else if (mode == GL_POINT)
		D3D_SetRenderState (D3DRS_FILLMODE, D3DFILL_POINT);
	else D3D_SetRenderState (D3DRS_FILLMODE, D3DFILL_SOLID);
}


void glShadeModel (GLenum mode)
{
	// easy peasy
	//D3D_SetRenderState (D3DRS_SHADEMODE, mode == GL_FLAT ? D3DSHADE_FLAT : D3DSHADE_GOURAUD);
}


void glGetIntegerv (GLenum pname, GLint *params)
{
	// here we only bother getting the values that glquake uses
	switch (pname)
	{
	case GL_MAX_TEXTURE_SIZE:
		// D3D allows both to be different so return the lowest
		params[0] = (d3d_Caps.MaxTextureWidth > d3d_Caps.MaxTextureHeight ? d3d_Caps.MaxTextureHeight : d3d_Caps.MaxTextureWidth);
		break;

	case GL_VIEWPORT:
		params[0] = d3d_Viewport.X;
		params[1] = d3d_Viewport.Y;
		params[2] = d3d_Viewport.Width;
		params[3] = d3d_Viewport.Height;
		break;

	case GL_STENCIL_BITS:
		if (d3d_PresentParams.AutoDepthStencilFormat == D3DFMT_D24S8)
			params[0] = 8;	 
		else params[0] = 0;

		break;

	default:
		params[0] = 0;
		return;
	}
}


LONG ChangeDisplaySettings_FakeGL (DWORD lpDevMode, DWORD dwflags)
{
 

	 
		return 1;
	 
}


void gluPerspective(	GLdouble  	fovy,
 	GLdouble  	aspect,
 	GLdouble  	zNear,
 	GLdouble  	zFar)
{

	D3DXMatrixPerspectiveFovRH(&d3d_CurrentMatrix->stack[d3d_CurrentMatrix->stackdepth], fovy, aspect, zNear, zFar);

	
}


/*
===================================================================================================================

			FOG

===================================================================================================================
*/

void glFogf (GLenum pname, GLfloat param)
{
	switch (pname)
	{
	case GL_FOG_MODE:
		glFogi (pname, param);
		break;

	case GL_FOG_DENSITY:
//		D3D_SetRenderState (D3DRS_FOGDENSITY, D3D_FloatToDWORD (param));
		break;

	case GL_FOG_START:
//		D3D_SetRenderState (D3DRS_FOGSTART, D3D_FloatToDWORD (param));
		break;

	case GL_FOG_END:
//		D3D_SetRenderState (D3DRS_FOGEND, D3D_FloatToDWORD (param));
		break;

	case GL_FOG_COLOR:
		break;

	default:
		break;
	}
}


void glFogfv (GLenum pname, const GLfloat *params)
{
	switch (pname)
	{
	case GL_FOG_COLOR:
//		D3D_SetRenderState (D3DRS_FOGCOLOR, GL_ColorToD3D (params[0], params[1], params[2], params[3]));
		break;

	default:
		glFogf (pname, params[0]);
		break;
	}
}


void glFogi (GLenum pname, GLint param)
{
	switch (pname)
	{
	case GL_FOG_MODE:
		// fixme - make these dependent on a glHint...
		if (param == GL_LINEAR)
		{
			if (d3d_Fog_Hint == GL_NICEST)
			{
//				D3D_SetRenderState (D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
//				D3D_SetRenderState (D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
			}
			else
			{
//				D3D_SetRenderState (D3DRS_FOGTABLEMODE, D3DFOG_NONE);
//				D3D_SetRenderState (D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
			}
		}
		else if (param == GL_EXP)
		{
			if (d3d_Fog_Hint == GL_NICEST)
			{
//				D3D_SetRenderState (D3DRS_FOGTABLEMODE, D3DFOG_EXP);
//				D3D_SetRenderState (D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
			}
			else
			{
	//			D3D_SetRenderState (D3DRS_FOGTABLEMODE, D3DFOG_NONE);
	//			D3D_SetRenderState (D3DRS_FOGVERTEXMODE, D3DFOG_EXP);
			}
		}
		else
		{
			if (d3d_Fog_Hint == GL_NICEST)
			{
//				D3D_SetRenderState (D3DRS_FOGTABLEMODE, D3DFOG_EXP2);
//				D3D_SetRenderState (D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
			}
			else
			{
//				D3D_SetRenderState (D3DRS_FOGTABLEMODE, D3DFOG_NONE);
//				D3D_SetRenderState (D3DRS_FOGVERTEXMODE, D3DFOG_EXP2);
			}
		}
		break;

	default:
		glFogf (pname, param);
		break;
	}
}


void glFogiv (GLenum pname, const GLint *params)
{
	switch (pname)
	{
	case GL_FOG_COLOR:
		// the spec isn't too clear on how these map to the fp values... oh well...
//		D3D_SetRenderState (D3DRS_FOGCOLOR, D3DCOLOR_ARGB (BYTE_CLAMP (params[3]), BYTE_CLAMP (params[0]), BYTE_CLAMP (params[1]), BYTE_CLAMP (params[2])));
		break;

	default:
		glFogi (pname, params[0]);
		break;
	}
}


/*
===================================================================================================================

			MULTITEXTURE

===================================================================================================================
*/

void WINAPI GL_ActiveTexture (GLenum texture)
{
	d3d_CurrentTMU = D3D_TMUForTexture (texture);
}


void WINAPI GL_MultiTexCoord2f (GLenum target, GLfloat s, GLfloat t)
{
	int TMU = D3D_TMUForTexture (target);

	d3d_CurrentTexCoord[TMU].s = s;
	d3d_CurrentTexCoord[TMU].t = t;
}


void WINAPI GL_MultiTexCoord1f (GLenum target, GLfloat s)
{
	int TMU = D3D_TMUForTexture (target);

	d3d_CurrentTexCoord[TMU].s = s;
	d3d_CurrentTexCoord[TMU].t = 0;
}


/*
===================================================================================================================

			EXTENSIONS

===================================================================================================================
*/

// we advertise ourselves as version 1.1 so that as few assumptions about capability as possible are made
static char *GL_VersionString = "1.1";

// these are the extensions we want to export
// right now GL_ARB_texture_env_combine is implemented but causes substantial perf hitches
// note: we advertise GL_EXT_texture_filter_anisotropic but we also do a double-check with the D3D caps when setting it for real
static char *GL_ExtensionString =
"GL_ARB_multitexture \
GL_ARB_texture_env_add \
GL_EXT_texture_filter_anisotropic ";

D3DADAPTER_IDENTIFIER9 d3d_AdapterID;

const GLubyte *glGetString (GLenum name)
{
	IDirect3D9_GetAdapterIdentifier (d3d_Object, 0, D3DENUM_NO_WHQL_LEVEL, &d3d_AdapterID);

	switch (name)
	{
	case GL_VENDOR:
		return d3d_AdapterID.Description;
	case GL_RENDERER:
		return d3d_AdapterID.Driver;
	case GL_VERSION:
		return GL_VersionString;
	case GL_EXTENSIONS:
	default:
		return GL_ExtensionString;
	}
}


// type cast unsafe conversion from
#pragma warning (push)
#pragma warning (disable: 4191)

typedef struct gld3d_entrypoint_s
{
	char *funcname;
	PROC funcproc;
} gld3d_entrypoint_t;

gld3d_entrypoint_t d3d_EntryPoints[] =
{
	{"glActiveTexture", (PROC) GL_ActiveTexture},
	{"glActiveTextureARB", (PROC) GL_ActiveTexture},
	{"glBindTexture", (PROC) glBindTexture},
	{"glMultiTexCoord1f", (PROC) GL_MultiTexCoord1f},
	{"glMultiTexCoord1fARB", (PROC) GL_MultiTexCoord1f},
	{"glMultiTexCoord2f", (PROC) GL_MultiTexCoord2f},
	{"glMultiTexCoord2fARB", (PROC) GL_MultiTexCoord2f},
	{"glClientActiveTexture", (PROC) GL_ClientActiveTexture},
	{"glClientActiveTextureARB", (PROC) GL_ClientActiveTexture},
	{NULL, NULL}
};


PROC WINAPI wglGetProcAddress (LPCSTR s)
{
	int i;

	// check all of our entrypoints
	for (i = 0; ; i++)
	{
		// no more entrypoints
		if (!d3d_EntryPoints[i].funcname) break;

		if (!strcmp (s, d3d_EntryPoints[i].funcname))
		{
			return d3d_EntryPoints[i].funcproc;
		}
	}

	// LocalDebugBreak();
	return 0;
}

#pragma warning (pop)

//#endif	// USEFAKEGL
