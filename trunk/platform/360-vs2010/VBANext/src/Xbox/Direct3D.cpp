/* Direct3D.cpp - written by OV2 */

#pragma comment( lib, "d3d9" )
#pragma comment( lib, "d3dx9" )
#pragma comment( lib, "DxErr9" )

#include <xtl.h>
#include <xui.h>

#include <tchar.h>
#include "direct3d.h"
#include "GeneralFunctions.h"
#include "Storage.h"
#include "RomList.h"

extern bool in_display_dlg;
extern bool exitFromXUI;
extern HXUIOBJ hScene;
extern HXUIOBJ phObj;
extern GameStorage snesStoreage;
extern "C" t_config config;

extern "C" int system_frame(int skip);
extern "C" t_bitmap bitmap;
extern void hq2x(uint8 *srcPtr, uint32 srcPitch, uint8 * /* deltaPtr */,
          uint8 *dstPtr, uint32 dstPitch, int width, int height);
extern void AdMame2x(uint8 *srcPtr, uint32 srcPitch, uint8 * /* deltaPtr */,
              uint8 *dstPtr, uint32 dstPitch, int width, int height);
extern void lq2x(uint8 *srcPtr, uint32 srcPitch, uint8 * /* deltaPtr */,
          uint8 *dstPtr, uint32 dstPitch, int width, int height);

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

/* CDirect3D::CDirect3D()
sets default values for the variables
*/

HANDLE RenderThread = NULL;

CDirect3D::CDirect3D()
{
 
	init_done = false;
	pD3D = NULL;
	pDevice = NULL;
	drawSurface = NULL;
	vertexBuffer = NULL;
	vertexBufferBG = NULL;
	afterRenderWidth = 0;
	afterRenderHeight =0;
	quadTextureSize = 0;
	fullscreen = true;
	iFilterScale = 0;

		/*if(!(RenderThread = (HANDLE)_beginthreadex(NULL,0,&SoundThread,(void *)this,0,NULL))) {
		MessageBox (GUI.hWnd, TEXT("Unable to create mixing thread, XAudio2 will not work."),
                        TEXT("Snes9X - XAudio2"),
                        MB_OK | MB_ICONWARNING);
		DeInitXAudio2();
		return false;*/
}

/* CDirect3D::~CDirect3D()
releases allocated objects
*/
CDirect3D::~CDirect3D()
{
	deInitialize();
}


/*  CDirect3D::initialize
initializes Direct3D (always in window mode)
IN:
hWnd	-	the HWND of the window in which we render/the focus window for fullscreen
-----
returns true if successful, false otherwise
*/
bool CDirect3D::initialize(D3DDevice *pDev)
{
 
	HRESULT hr;
	pDevice = pDev;
	VOID* pCode = NULL;
    DWORD dwSize = 0;

	ZeroMemory(&dPresentParams, sizeof(dPresentParams));	  
	dPresentParams.BackBufferFormat = D3DFMT_X8R8G8B8; 
	dPresentParams.BackBufferWidth = 1280;
	dPresentParams.BackBufferHeight = 720;
	dPresentParams.PresentationInterval = D3DPRESENT_INTERVAL_ONE ;
 
	pDevice->Reset(&dPresentParams);
 

	    // Compile pixel shader.
    ID3DXBuffer* pPixelShaderCode;
    ID3DXBuffer*  pPixelErrorMsg;
    hr = D3DXCompileShader( g_strPixelShaderProgram,
                            ( UINT )strlen( g_strPixelShaderProgram ),
                            NULL,
                            NULL,
                            "main",
                            "ps_2_0",
                            0,
                            &pPixelShaderCode,
                            &pPixelErrorMsg,
                            NULL );
    if( FAILED( hr ) )
    {
        if( pPixelErrorMsg )
            OutputDebugString( ( char* )pPixelErrorMsg->GetBufferPointer() );
        return E_FAIL;
    }

    // Create pixel shader.
    pDevice->CreatePixelShader( ( DWORD* )pPixelShaderCode->GetBufferPointer(),
                                     &g_pPixelShader );

	    // Create vertex declaration
    if( NULL == g_pGradientVertexDecl )
    {
        static const D3DVERTEXELEMENT9 decl[] =
        {
			{ 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			{ 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };

        if( FAILED( pDevice->CreateVertexDeclaration( decl, &g_pGradientVertexDecl ) ) )
            return E_FAIL;
    }
 
	    // Create vertex shader
    if( NULL == g_pGradientVertexShader )
    {
        ID3DXBuffer* pShaderCode;
        if( FAILED( D3DXCompileShader( g_strGradientShader, strlen( g_strGradientShader ),
                                       NULL, NULL, "GradientVertexShader", "vs_2_0", 0,
                                       &pShaderCode, NULL, NULL ) ) )
            return false;

        if( FAILED( pDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                                                      &g_pGradientVertexShader ) ) )
            return false;

		if (pShaderCode)
		{
			pShaderCode->Release();
			pShaderCode = NULL;
		}
    }

	if (NULL == vertexDeclaration)
	{
		pDevice->CreateVertexDeclaration(decl,&vertexDeclaration);
	}

	if (NULL == vertexBuffer)
	{
		hr = pDevice->CreateVertexBuffer(sizeof(triangleStripVertices),D3DUSAGE_CPU_CACHED_MEMORY,0,0,&vertexBuffer,NULL);
		if(FAILED(hr)) {
			DXTRACE_ERR_MSGBOX(TEXT("Error creating vertex buffer"), hr);
			return false;
		}
	}

	//if (NULL == vertexBufferBG)
	//{
	//	hr = pDevice->CreateVertexBuffer(sizeof(triangleStripVertices),D3DUSAGE_CPU_CACHED_MEMORY,0,0,&vertexBufferBG,NULL);
	//	if(FAILED(hr)) {
	////		DXTRACE_ERR_MSGBOX(TEXT("Error creating vertex buffer"), hr);
	//		return false;
	//	}

	//}

	//D3DXCompileShaderFromFile("game:\\Media\\Shaders\\Super2xSaI.fx",NULL,NULL,"S_FRAGMENT","ps_3_0",D3DXSHADER_MICROCODE_BACKEND_NEW,&ppShader,NULL,NULL);
	 
	//hr = pDevice->CreatePixelShader((DWORD*)ppShader->GetBufferPointer(),&g_pPixelShader);
	  
	//VideoMemory controls if we want bilinear filtering
	 
	if (config.pointfilter == 1)
	{
		pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	}
	else
	{
		pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	}
	 

	pDevice->SetVertexShader( g_pGradientVertexShader );		
	pDevice->SetPixelShader( g_pPixelShader );

    pDevice->SetVertexDeclaration( g_pGradientVertexDecl );
	pDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );

    
 
    // hr = D3DXCreateTextureFromFile( pDevice, 
	//	"game:\\media\\ingame.png",                                                
    //                                          &backTexture );

	
  
		changeDrawSurfaceSize(2);
 

	init_done = true;

	return true;

}
 

void CDirect3D::deInitialize()
{
	pDevice->UnsetAll();

	destroyDrawSurface();
 
	if (g_pGradientVertexDecl)
	{
		g_pGradientVertexDecl->Release();
		g_pGradientVertexDecl = NULL;
	}

	if (g_pGradientVertexShader)
	{
		g_pGradientVertexShader->Release();
		g_pGradientVertexShader = NULL;

	}

	if (g_pPixelShader)
	{
		g_pPixelShader->Release();
		g_pPixelShader = NULL;

	}

	if(vertexBuffer) {
		vertexBuffer->Release();
		vertexBuffer = NULL;
	}

	//if(vertexBufferBG) {
	//	vertexBufferBG->Release();
	//	vertexBufferBG = NULL;
	//}

	if (vertexDeclaration)
	{
		vertexDeclaration->Release();
		vertexDeclaration = NULL;
	}


	init_done = false;
	afterRenderWidth = 0;
	afterRenderHeight = 0;
	quadTextureSize = 0;
	fullscreen = false;
	iFilterScale = 0;
}

/*  CDirect3D::render
does the actual rendering, changes the draw surface if necessary and recalculates
the vertex information if filter output size changes
IN:
Src		-	the input surface
*/
void CDirect3D::render()
{
	 	
	int iNewFilterScale;
	D3DLOCKED_RECT lr;
	D3DLOCKED_RECT lrConv;
	HRESULT hr;

	if(!init_done) return;
 
	if(FAILED(hr = drawSurface->LockRect(0, &lr, NULL, D3DLOCK_NOOVERWRITE))) {
		DXTRACE_ERR_MSGBOX( TEXT("Unable to lock texture"), hr);
		return;
	} else {
  
		system_frame(0); 	
		if (config.bilinear == 1)
		{
			lq2x(bitmap.data,bitmap.pitch,0,(unsigned char *)lr.pBits, lr.Pitch, bitmap.viewport.w,bitmap.viewport.h);		 	
		}
		else
		{
			AdMame2x(bitmap.data,bitmap.pitch,0,(unsigned char *)lr.pBits, lr.Pitch, bitmap.viewport.w,bitmap.viewport.h); 	
		}
		
		drawSurface->UnlockRect(0);
	}

		//if the output size of the render method changes we need new vertices
	if(afterRenderWidth != bitmap.viewport.w << 1 || afterRenderHeight != bitmap.viewport.h << 1) {
		afterRenderHeight = bitmap.viewport.h << 1;
		afterRenderWidth = bitmap.viewport.w << 1;
		pDevice->SetStreamSource(0,NULL,0,0);
		setupVertices();
	}
 
 
	pDevice->Clear(0, NULL, D3DCLEAR_TARGET,  D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
	
 
	
    pDevice->SetTexture( 0, drawSurface); 
	pDevice->SetStreamSource(0,vertexBuffer,0,sizeof(VERTEX));		
	pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP,0,2); 	 
	pDevice->Present(NULL, NULL, NULL, NULL);
	
   
	return;
}

void CDirect3D::ClearTargets()
{

	pDevice->SetStreamSource(0,NULL,0,NULL);		
	pDevice->SetTexture( 0, NULL); 
	initialize(pDevice);
}

 
/*  CDirect3D::createDrawSurface
calculates the necessary texture size (multiples of 2)
and creates a new texture
*/
void CDirect3D::createDrawSurface()
{
	int neededSize;
	HRESULT hr;

	//we need at least 512 pixels (SNES_WIDTH * 2) so we can start with that value
	quadTextureSize = 512;
	neededSize = 512;
	while(quadTextureSize < neededSize)
		quadTextureSize *=2;

	if(!drawSurface) {
		hr = pDevice->CreateTexture(
			bitmap.width, bitmap.height,
			1, // 1 level, no mipmaps
			D3DUSAGE_CPU_CACHED_MEMORY , // dynamic textures can be locked
			D3DFMT_LIN_R5G6B5,
			0,
			&drawSurface,
			NULL );

		if(FAILED(hr)) {
			DXTRACE_ERR_MSGBOX(TEXT("Error while creating texture"), hr);
			return;
		}

		blankTexture(drawSurface);
	}

 
}

/*  CDirect3D::destroyDrawSurface
releases the old textures (if allocated)
*/
void CDirect3D::destroyDrawSurface()
{
	if(drawSurface) {
		drawSurface->Release();
		drawSurface = NULL;
	}


}

/*  CDirect3D::blankTexture
clears a texture (fills it with zeroes)
IN:
texture		-	the texture to be blanked
-----
returns true if successful, false otherwise
*/
bool CDirect3D::blankTexture(LPDIRECT3DTEXTURE9 texture)
{
	D3DLOCKED_RECT lr;
	HRESULT hr;

	if(FAILED(hr = texture->LockRect(0, &lr, NULL, 0))) {
		DXTRACE_ERR_MSGBOX( TEXT("Unable to lock texture"), hr);
		return false;
	} else {
		memset(lr.pBits, 0, lr.Pitch * bitmap.height);
		texture->UnlockRect(0);
		return true;
	}
	 
}

/*  CDirect3D::changeDrawSurfaceSize
changes the draw surface size: deletes the old textures, creates a new texture
and calculate new vertices
IN:
iScale	-	the scale that has to fit into the textures
-----
returns true if successful, false otherwise
*/
bool CDirect3D::changeDrawSurfaceSize(int iScale)
{
	iFilterScale = iScale;

	if(pDevice) {
		destroyDrawSurface();
		createDrawSurface();
		setupVertices();
		return true;
	}
	return false;
}

/*  CDirect3D::setupVertices
calculates the vertex coordinates
(respecting the stretch and aspect ratio settings)
*/
void CDirect3D::setupVertices()
{
	float xFactor;
	float yFactor;
	float minFactor;
	float renderWidthCalc,renderHeightCalc;
	RECT drawRect;
	 
	void *pLockedVertexBuffer;
	void *pLockedVertexBufferBG;
	


	if (config.aspect==0)
	{
		drawRect.top = 0;
		drawRect.left = 0;
		drawRect.right = dPresentParams.BackBufferWidth;
		drawRect.bottom = dPresentParams.BackBufferHeight;

	}
	else
	{
		float snesAspect = (float)bitmap.viewport.w /bitmap.viewport.h;

		renderWidthCalc = (float)afterRenderWidth;
		renderHeightCalc = (float)afterRenderHeight;
		if(renderWidthCalc/renderHeightCalc>snesAspect)
			renderWidthCalc = renderHeightCalc * snesAspect;
		else if(renderWidthCalc/renderHeightCalc<snesAspect)
			renderHeightCalc = renderWidthCalc / snesAspect;

		xFactor = (float)dPresentParams.BackBufferWidth / renderWidthCalc;
		yFactor = (float)dPresentParams.BackBufferHeight / renderHeightCalc;
		minFactor = xFactor < yFactor ? xFactor : yFactor;

		drawRect.right = (LONG)(renderWidthCalc * minFactor);
		drawRect.bottom = (LONG)(renderHeightCalc * minFactor);

		drawRect.left = (dPresentParams.BackBufferWidth - drawRect.right) / 2;
		drawRect.top = (dPresentParams.BackBufferHeight - drawRect.bottom) / 2;

		drawRect.right += drawRect.left;
		drawRect.bottom += drawRect.top;


	}

		float tX = (float)afterRenderWidth / bitmap.width;
		float tY = (float)afterRenderHeight / bitmap.height;

	//we need to substract -0.5 from the x/y coordinates to match texture with pixel space
	//see http://msdn.microsoft.com/en-us/library/bb219690(VS.85).aspx
	triangleStripVertices[0] = VERTEX((float)drawRect.left - 0.5f,(float)drawRect.bottom - 0.5f,0.0f,1.0f,0.0f,tY);
	triangleStripVertices[1] = VERTEX((float)drawRect.left - 0.5f,(float)drawRect.top - 0.5f,0.0f,1.0f,0.0f,0.0f);
	triangleStripVertices[2] = VERTEX((float)drawRect.right - 0.5f,(float)drawRect.bottom - 0.5f,0.0f,1.0f,tX,tY);
	triangleStripVertices[3] = VERTEX((float)drawRect.right - 0.5f,(float)drawRect.top - 0.5f,0.0f,1.0f,tX,0.0f);

	/*triangleStripVerticesBG[0] = VERTEX((float)0 - 0.5f,(float)dPresentParams.BackBufferHeight - 0.5f,0.0f,1.0f,0.0f,1);
	triangleStripVerticesBG[1] = VERTEX((float)0 - 0.5f,(float)0 - 0.5f,0.0f,1.0f,0.0f,0.0f);
	triangleStripVerticesBG[2] = VERTEX((float)dPresentParams.BackBufferWidth - 0.5f,(float)dPresentParams.BackBufferHeight - 0.5f,0.0f,1.0f,tX,1);
	triangleStripVerticesBG[3] = VERTEX((float)dPresentParams.BackBufferWidth - 0.5f,(float)0 - 0.5f,0.0f,1.0f,1,0.0f);*/

	HRESULT hr = vertexBuffer->Lock(0,0,&pLockedVertexBuffer,NULL);
	memcpy(pLockedVertexBuffer,triangleStripVertices,sizeof(triangleStripVertices));
	vertexBuffer->Unlock();

	//hr = vertexBufferBG->Lock(0,0,&pLockedVertexBufferBG,NULL);
	//memcpy(pLockedVertexBufferBG,triangleStripVerticesBG,sizeof(triangleStripVerticesBG));
	//vertexBufferBG->Unlock();
}

/*  CDirect3D::changeRenderSize
determines if we need to reset the device (if the size changed)
called with (0,0) whenever we want new settings to take effect
IN:
newWidth,newHeight	-	the new window size
-----
returns true if successful, false otherwise
*/
bool CDirect3D::changeRenderSize(unsigned int newWidth, unsigned int newHeight)
{
	if(!init_done)
		return false;

	//if we already have the wanted size no change is necessary
	//during fullscreen no changes are allowed
	if(fullscreen || dPresentParams.BackBufferWidth == newWidth && dPresentParams.BackBufferHeight == newHeight)
		return true;

	if(!resetDevice())
		return false;
	setupVertices();
	return true;
}

/*  CDirect3D::resetDevice
resets the device
called if surface was lost or the settings/display size require a device reset
-----
returns true if successful, false otherwise
*/
bool CDirect3D::resetDevice()
{
	if(!pDevice) return false;

	HRESULT hr;

	//release prior to reset
	destroyDrawSurface();

	//zero or unknown values result in the current window size/display settings
	dPresentParams.BackBufferWidth = 1280;
	dPresentParams.BackBufferHeight = 720;
	dPresentParams.BackBufferCount = 1;
	dPresentParams.BackBufferFormat = D3DFMT_X8R8G8B8;  
	dPresentParams.Windowed = false;
	dPresentParams.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	
 

	if(FAILED(hr = pDevice->Reset(&dPresentParams))) {
		DXTRACE_ERR(TEXT("Unable to reset device"), hr);
		return false;
	}

	//VideoMemory controls if we want bilinear filtering
    pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	pDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
	
	//recreate the surface
	createDrawSurface();
	return true;
}
 
/*  CDirect3D::setFullscreen
enables/disables fullscreen mode
IN:
fullscreen	-	determines if fullscreen is enabled/disabled
-----
returns true if successful, false otherwise
*/
bool CDirect3D::setFullscreen(bool fullscreen)
{
	if(!init_done)
		return false;

 
	this->fullscreen = fullscreen;
	if(!resetDevice())
		return false;

	//present here to get a fullscreen blank even if no rendering is done
	pDevice->Present(NULL,NULL,NULL,NULL);
	setupVertices();
	return true;
}
