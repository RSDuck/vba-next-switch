// VBANext.cpp : Defines the entry point for the application.
//

#include "stdafx.h"

//-------------------------------------------------------------------------------------
// Vertex shader
// We use the register semantic here to directly define the input register
// matWVP.  Conversely, we could let the HLSL compiler decide and check the
// constant table.
//-------------------------------------------------------------------------------------
const char* g_strVertexShaderProgram = 
" float4x4 matWVP : register(c0);              "  
"                                              "  
" struct VS_IN                                 "  
" {                                            " 
"     float4 ObjPos   : POSITION;              "  // Object space position 
"     float4 Color    : COLOR;                 "  // Vertex color                 
" };                                           " 
"                                              " 
" struct VS_OUT                                " 
" {                                            " 
"     float4 ProjPos  : POSITION;              "  // Projected space position 
"     float4 Color    : COLOR;                 "  
" };                                           "  
"                                              "  
" VS_OUT main( VS_IN In )                      "  
" {                                            "  
"     VS_OUT Out;                              "  
"     Out.ProjPos = mul( matWVP, In.ObjPos );  "  // Transform vertex into
"     Out.Color = In.Color;                    "  // Projected space and 
"     return Out;                              "  // Transfer color
" }                                            ";

//-------------------------------------------------------------------------------------
// Pixel shader
//-------------------------------------------------------------------------------------
const char* g_strPixelShaderProgram = 
" struct PS_IN                                 "
" {                                            "
"     float4 Color : COLOR;                    "  // Interpolated color from                      
" };                                           "  // the vertex shader
"                                              "  
" float4 main( PS_IN In ) : COLOR              "  
" {                                            "  
"     return In.Color;                         "  // Output color
" }                                            "; 

//-------------------------------------------------------------------------------------
// Structure to hold vertex data.
//-------------------------------------------------------------------------------------
struct COLORVERTEX
{
    float       Position[3];
    DWORD       Color;
};

//-------------------------------------------------------------------------------------
// Time             Since fAppTime is a float, we need to keep the quadword app time 
//                  as a LARGE_INTEGER so that we don't lose precision after running
//                  for a long time.
//-------------------------------------------------------------------------------------
struct TimeInfo
{    
    LARGE_INTEGER qwTime;    
    LARGE_INTEGER qwAppTime;   

    float fAppTime;    
    float fElapsedTime;    

    float fSecsPerTick;    
};

//-------------------------------------------------------------------------------------
// Global variables
//-------------------------------------------------------------------------------------
D3DDevice*             g_pd3dDevice;    // Our rendering device
D3DVertexBuffer*       g_pVB;           // Buffer to hold vertices
D3DVertexDeclaration*  g_pVertexDecl;   // Vertex format decl
D3DVertexShader*       g_pVertexShader; // Vertex Shader
D3DPixelShader*        g_pPixelShader;  // Pixel Shader

XMMATRIX g_matWorld;
XMMATRIX g_matProj;
XMMATRIX g_matView;

TimeInfo g_Time;

BOOL g_bWidescreen = TRUE;

//-------------------------------------------------------------------------------------
// Name: InitTime()
// Desc: Initializes the timer variables
//-------------------------------------------------------------------------------------
void InitTime()
{    

    // Get the frequency of the timer
    LARGE_INTEGER qwTicksPerSec;
    QueryPerformanceFrequency( &qwTicksPerSec );
    g_Time.fSecsPerTick = 1.0f / (float)qwTicksPerSec.QuadPart;

    // Save the start time
    QueryPerformanceCounter( &g_Time.qwTime );
    
    // Zero out the elapsed and total time
    g_Time.qwAppTime.QuadPart = 0;
    g_Time.fAppTime = 0.0f; 
    g_Time.fElapsedTime = 0.0f;    
}


//-------------------------------------------------------------------------------------
// Name: InitD3D()
// Desc: Initializes Direct3D
//-------------------------------------------------------------------------------------
HRESULT InitD3D()
{
    // Create the D3D object.
    Direct3D* pD3D = Direct3DCreate9( D3D_SDK_VERSION );
    if( !pD3D )
        return E_FAIL;

    // Set up the structure used to create the D3DDevice.
    D3DPRESENT_PARAMETERS d3dpp; 
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    XVIDEO_MODE VideoMode;
    XGetVideoMode( &VideoMode );
    g_bWidescreen = VideoMode.fIsWideScreen;
    d3dpp.BackBufferWidth        = min( VideoMode.dwDisplayWidth, 1280 );
    d3dpp.BackBufferHeight       = min( VideoMode.dwDisplayHeight, 720 );
    d3dpp.BackBufferFormat       = D3DFMT_X8R8G8B8;
    d3dpp.BackBufferCount        = 1;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_ONE;

    // Create the Direct3D device.
    if( FAILED( pD3D->CreateDevice( 0, D3DDEVTYPE_HAL, NULL,
                                    D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                    &d3dpp, &g_pd3dDevice ) ) )
        return E_FAIL;

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Name: InitScene()
// Desc: Creates the scene.  First we compile our shaders. For the final version
//       of a game, you should store the shaders in binary form; don't call 
//       D3DXCompileShader at runtime. Next, we declare the format of our 
//       vertices, and then create a vertex buffer. The vertex buffer is basically
//       just a chunk of memory that holds vertices. After creating it, we must 
//       Lock()/Unlock() it to fill it. Finally, we set up our world, projection,
//       and view matrices.
//-------------------------------------------------------------------------------------
HRESULT InitScene()
{
    // Compile vertex shader.
    ID3DXBuffer* pVertexShaderCode;
    ID3DXBuffer* pVertexErrorMsg;
    HRESULT hr = D3DXCompileShader( g_strVertexShaderProgram, 
                                    (UINT)strlen( g_strVertexShaderProgram ),
                                    NULL, 
                                    NULL, 
                                    "main", 
                                    "vs_2_0", 
                                    0, 
                                    &pVertexShaderCode, 
                                    &pVertexErrorMsg, 
                                    NULL );
    if( FAILED(hr) )
    {
        if( pVertexErrorMsg )
            OutputDebugString( (char*)pVertexErrorMsg->GetBufferPointer() );
        return E_FAIL;
    }    

    // Create vertex shader.
    g_pd3dDevice->CreateVertexShader( (DWORD*)pVertexShaderCode->GetBufferPointer(), 
                                      &g_pVertexShader );

    // Compile pixel shader.
    ID3DXBuffer* pPixelShaderCode;
    ID3DXBuffer* pPixelErrorMsg;
    hr = D3DXCompileShader( g_strPixelShaderProgram, 
                            (UINT)strlen( g_strPixelShaderProgram ),
                            NULL, 
                            NULL, 
                            "main", 
                            "ps_2_0", 
                            0, 
                            &pPixelShaderCode, 
                            &pPixelErrorMsg,
                            NULL );
    if( FAILED(hr) )
    {
        if( pPixelErrorMsg )
            OutputDebugString( (char*)pPixelErrorMsg->GetBufferPointer() );
        return E_FAIL;
    }

    // Create pixel shader.
    g_pd3dDevice->CreatePixelShader( (DWORD*)pPixelShaderCode->GetBufferPointer(), 
                                     &g_pPixelShader );
    
    // Define the vertex elements and
    // Create a vertex declaration from the element descriptions.
    D3DVERTEXELEMENT9 VertexElements[3] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
        D3DDECL_END()
    };
    g_pd3dDevice->CreateVertexDeclaration( VertexElements, &g_pVertexDecl );

    // Create the vertex buffer. Here we are allocating enough memory
    // (from the default pool) to hold all our 3 custom vertices. 
    if( FAILED( g_pd3dDevice->CreateVertexBuffer( 3*sizeof(COLORVERTEX),
                                                  D3DUSAGE_WRITEONLY, 
                                                  NULL,
                                                  D3DPOOL_MANAGED, 
                                                  &g_pVB, 
                                                  NULL ) ) )
        return E_FAIL;

    // Now we fill the vertex buffer. To do this, we need to Lock() the VB to
    // gain access to the vertices. This mechanism is required because the
    // vertex buffer may still be in use by the GPU. This can happen if the
    // CPU gets ahead of the GPU. The GPU could still be rendering the previous
    // frame.
    COLORVERTEX g_Vertices[] =
    {
        {  0.0f, -1.1547f, 0.0f, 0xffff0000 }, // x, y, z, color
        { -1.0f,  0.5777f, 0.0f, 0xff00ff00 },
        {  1.0f,  0.5777f, 0.0f, 0xffffff00 },
    };

    COLORVERTEX* pVertices;
    if( FAILED( g_pVB->Lock( 0, 0, (void**)&pVertices, 0 ) ) )
        return E_FAIL;
    memcpy( pVertices, g_Vertices, 3*sizeof(COLORVERTEX) );
    g_pVB->Unlock();

    // Initialize the world matrix
    g_matWorld = XMMatrixIdentity();

    // Initialize the projection matrix
    FLOAT fAspect = ( g_bWidescreen ) ? (16.0f / 9.0f) : (4.0f / 3.0f); 
    g_matProj = XMMatrixPerspectiveFovLH( XM_PIDIV4, fAspect, 1.0f, 200.0f );

    // Initialize the view matrix
    XMVECTOR vEyePt    = { 0.0f, 0.0f,-7.0f, 0.0f };
    XMVECTOR vLookatPt = { 0.0f, 0.0f, 0.0f, 0.0f };
    XMVECTOR vUp       = { 0.0f, 1.0f, 0.0f, 0.0f };
    g_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUp );

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Name: UpdateTime()
// Desc: Updates the elapsed time since our last frame.
//-------------------------------------------------------------------------------------
void UpdateTime()
{
    LARGE_INTEGER qwNewTime;
    LARGE_INTEGER qwDeltaTime;
    
    QueryPerformanceCounter( &qwNewTime );    
    qwDeltaTime.QuadPart = qwNewTime.QuadPart - g_Time.qwTime.QuadPart;

    g_Time.qwAppTime.QuadPart += qwDeltaTime.QuadPart;    
    g_Time.qwTime.QuadPart     = qwNewTime.QuadPart;
    
    g_Time.fElapsedTime      = g_Time.fSecsPerTick * ((FLOAT)(qwDeltaTime.QuadPart));
    g_Time.fAppTime          = g_Time.fSecsPerTick * ((FLOAT)(g_Time.qwAppTime.QuadPart));    
}


//-------------------------------------------------------------------------------------
// Name: Update()
// Desc: Updates the world for the next frame
//-------------------------------------------------------------------------------------
void Update()
{
    // Set the world matrix
    float fAngle = fmodf( -g_Time.fAppTime, XM_2PI );
    static const XMVECTOR vAxisZ = { 0, 0, 1.0f, 0 };
    g_matWorld = XMMatrixRotationAxis( vAxisZ, fAngle );
}


//-------------------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the scene
//-------------------------------------------------------------------------------------
void Render()
{
    // Clear the backbuffer to a blue color
    g_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, 
                         D3DCOLOR_XRGB(0,0,255), 1.0f, 0L );

    // Draw the triangles in the vertex buffer. This is broken into a few steps:
    
    // We are passing the vertices down a "stream", so first we need
    // to specify the source of that stream, which is our vertex buffer. 
    // Then we need to let D3D know what vertex and pixel shaders to use. 
    g_pd3dDevice->SetVertexDeclaration( g_pVertexDecl );
    g_pd3dDevice->SetStreamSource( 0, g_pVB, 0, sizeof(COLORVERTEX) );
    g_pd3dDevice->SetVertexShader( g_pVertexShader );
    g_pd3dDevice->SetPixelShader( g_pPixelShader );
   
    // Build the world-view-projection matrix and pass it into the vertex shader
    XMMATRIX matWVP = g_matWorld * g_matView * g_matProj;
    g_pd3dDevice->SetVertexShaderConstantF( 0, (FLOAT*)&matWVP, 4 );

    // Draw the vertices in the vertex buffer
    g_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, 1 );

    // Present the backbuffer contents to the display
    g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
}


//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------
void __cdecl main()
{
    // Initialize Direct3D
    if( FAILED( InitD3D() ) )
        return;

    // Initialize the vertex buffer
    if( FAILED( InitScene() ) )
        return;

    InitTime();

    for(;;) // loop forever
    {
        // What time is it?
        UpdateTime();
        // Update the world
        Update();   
        // Render the scene
        Render();
    }
}

