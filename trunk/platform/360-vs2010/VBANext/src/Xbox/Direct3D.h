/* Direct3D.h - written by OV2 */
#ifndef W9XDIRECT3D_H
#define W9XDIRECT3D_H

#include <xtl.h>
#include <d3d9.h>
#include <vector>
#include "types.h"
 
 
typedef struct _VERTEX {
		float x, y, z;
		float rhw;
		float tx, ty;
		_VERTEX() {}
		_VERTEX(float x,float y,float z,float rhw,float tx,float ty) {
			this->x=x;this->y=y;this->z=z;this->rhw=rhw;this->tx=tx;this->ty=ty;
		}
} VERTEX; //our custom vertex with a constuctor for easier assignment


typedef struct
{
  uint8 *data;      /* Bitmap data */
  int width;        /* Bitmap width (32+512+32) */
  int height;       /* Bitmap height (256) */
  int depth;        /* Color depth (8 bits) */
  int pitch;        /* Width of bitmap in bytes */
  int granularity;  /* Size of each pixel in bytes */
  int remap;        /* 1= Translate pixel data */
  struct
  {
    int x;          /* X offset of viewport within bitmap */
    int y;          /* Y offset of viewport within bitmap */
    int w;          /* Width of viewport */
    int h;          /* Height of viewport */
    int ow;         /* Previous width of viewport */
    int oh;         /* Previous height of viewport */
    int changed;    /* 1= Viewport width or height have changed */
  } viewport;
} t_bitmap;
 
class CDirect3D
{
private:
	bool                  init_done;					//has initialize been called?
	LPDIRECT3D9           pD3D;
	
	

	LPDIRECT3DVERTEXBUFFER9 vertexBuffer;
	LPDIRECT3DVERTEXBUFFER9 vertexBufferBG;
	LPDIRECT3DVERTEXDECLARATION9 vertexDeclaration;
	D3DVertexShader* g_pGradientVertexShader;
	D3DVertexDeclaration* g_pGradientVertexDecl;
	D3DPixelShader* g_pPixelShader;
	LPD3DXBUFFER ppShader;
	D3DPRESENT_PARAMETERS dPresentParams;
	int iFilterScale;									//the current maximum filter scale (at least 2)
	unsigned int afterRenderWidth, afterRenderHeight;	//dimensions after filter has been applied
	unsigned int quadTextureSize;						//size of the texture (only multiples of 2)
	bool fullscreen;									//are we currently displaying in fullscreen mode
	
	VERTEX triangleStripVertices[4];					//the 4 vertices that make up our display rectangle
	VERTEX triangleStripVerticesBG[4];					//the 4 vertices that make up our display rectangle

	bool blankTexture(LPDIRECT3DTEXTURE9 texture);
	
	LPDIRECT3DTEXTURE9 backTexture;
	bool changeDrawSurfaceSize(int iScale);
	
	bool resetDevice();
 
 
public:
	CDirect3D();
	~CDirect3D();
	LPDIRECT3DDEVICE9     pDevice;
	bool initialize(D3DDevice *pDevice);
	void deInitialize();
	void render();
	void renderTextureOnly();
	bool changeRenderSize(unsigned int newWidth, unsigned int newHeight);
	bool setFullscreen(bool fullscreen);
	void setSnes9xColorFormat();

	LPDIRECT3DTEXTURE9    drawSurface;					//the texture used for all drawing operations
	void createDrawSurface();
	void destroyDrawSurface();
	void setupVertices();
	void ClearTargets();
 
};

#endif