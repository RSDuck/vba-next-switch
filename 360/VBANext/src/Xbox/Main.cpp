#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>
#include <xfilecache.h>

#include "Main.h"
#include "RomList.h"
#include "Favorites.h"
#include "InGameOptions.h"
#include "Storage.h"
#include "RomSettings.h"
 
static const unsigned long  ACHIEVEMENT_COUNT = 8;
 
struct AchievementPicture
{
    IDirect3DTexture9* Texture;		// Texture of the picture
    unsigned long dwImageId;		// Id of the picture
};

extern "C" LPDIRECT3D9 D3D;
extern "C" LPDIRECT3DDEVICE9 D3D_Device;

t_config config;

 
AchievementPicture gAchievement;

GameStorage xStorage;
char    GamerName[ 256 ];

BYTE* m_Achievements;
unsigned long dwAchievementCount = 0;
VOID EnumerateAchievements();
 
//--------------------------------------------------------------------------------------
// Scene implementation class.
//--------------------------------------------------------------------------------------
class CVBA360Menu : public CXuiSceneImpl
{

	public:
	CXuiControl m_SignInLabel;
	protected:
	CXuiControl m_RomList;
	CXuiControl m_MusicToggle;
	CXuiControl m_button2;
	CXuiControl m_button3;
	CXuiControl m_button4;
	CXuiControl m_button5;
	CXuiControl m_Storage;
	CXuiControl m_ToggleSixButton;
	//CXuiVideo   m_BackVideo;

	//CXuiImage m_Logo;


	// Message map.
	XUI_BEGIN_MSG_MAP()
		XUI_ON_XM_INIT( OnInit )
		XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
		XUI_ON_XM_RENDER( OnRender )
		XUI_END_MSG_MAP()

	public:
		void SetSignedInName()
		{
			wchar_t w_gamertag[XUSER_NAME_SIZE];
			wchar_t w_gamertagText[255];
			MultiByteToWideChar(CP_ACP, 0, GamerName, -1, w_gamertag, XUSER_NAME_SIZE);

			if (strlen(GamerName) == 0)
				swprintf_s(w_gamertagText, L"Not Signed in");
			else
				swprintf_s(w_gamertagText,L"Signed in as : %S",GamerName);

			m_SignInLabel.SetText(w_gamertagText);		  
		}

		// Handler for the XM_NOTIFY message
		long OnNotifyPress( HXUIOBJ hObjPressed, int& bHandled )
		{

			if( hObjPressed == m_button4 )
			{
				const WCHAR * button_text = L"OK";
				ShowMessageBoxEx(NULL,NULL,L"About", L"Visual Boy Advance 360 V0.03 Beta\n\nFor fun and friends only\n\n", 1, (LPCWSTR*)&button_text,NULL,  XUI_MB_CENTER_ON_PARENT, NULL);


			}
			else if (hObjPressed == m_MusicToggle)
			{
			}
			else if (hObjPressed == m_button5 )
			{
				// Quit to Dash
				XLaunchNewImage( "", 0 );

			}
			else if (hObjPressed == m_Storage)
			{
				xStorage.ShowDeviceUI();
				return S_OK;
			}
			else if (hObjPressed == m_button3)
			{
				XShowAchievementsUI( xStorage.GetSignedInUser() );
			}
			else
			{
				return S_OK;
			}

			bHandled = TRUE;
			return S_OK;
		}

		long OnRender(XUIMessageRender *pRenderData, int &bHandled)
		{
			XUserGetName(xStorage.GetSignedInUser(), GamerName, sizeof(GamerName));
			SetSignedInName();

			return S_OK;
		}


		//----------------------------------------------------------------------------------
		// Performs initialization tasks - retreives controls.
		//----------------------------------------------------------------------------------
		long OnInit( XUIMessageInit* pInitData, int& bHandled )
		{

			// Retrieve controls for later use.
			GetChildById( L"XuiRoms", &m_RomList );
			GetChildById( L"XuiMusicToggle", &m_MusicToggle );
			GetChildById( L"XuiFavorites", &m_button2 );
			GetChildById( L"XuiOptions", &m_button3 );
			GetChildById( L"XuiAbout", &m_button4 );
			GetChildById( L"XuiQuit", &m_button5 );
			GetChildById( L"XuiStorageDevice", &m_Storage );
			//GetChildById( L"XuiBackVideo", &m_BackVideo );
			GetChildById( L"XuiLabelSignInInfo", &m_SignInLabel );
			//GetChildById( L"XuiLogo", &m_Logo );

			//m_BackVideo.SetVolume(-96.0);
			//m_BackVideo.Play( L"file://game:/media/VBASkin.xzp#Skin\\GenesisVideo.wmv" );
			return S_OK;
		}

	public:

		// Define the class. The class name must match the ClassOverride property
		// set for the scene in the UI Authoring tool.
		XUI_IMPLEMENT_CLASS( CVBA360Menu, L"MainScene", XUI_CLASS_SCENE )
};

 

long RenderGame( IDirect3DDevice9 *pDevice )
{
	// Render game graphics.
	pDevice->Clear(
			0,
			NULL,
			D3DCLEAR_TARGET | D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER,
			D3DCOLOR_ARGB( 255, 0, 0, 0 ),
			1.0,
			0 );

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: RegisterXuiClasses
// Desc: Registers all the scene classes.
//--------------------------------------------------------------------------------------
long CVBA360App::RegisterXuiClasses()
{
	// We must register the video control classes
	XuiHtmlRegister();
	XuiVideoRegister();
	XuiSoundXAudioRegister();
	CVBA360Menu::Register();
	CRomListScene::Register();
	CRomList::Register();
	CInGameOptions::Register();
	CFavoritesList::Register();
	CFavoritesListScene::Register();

	// Register any other classes necessary for the app/scene
	return S_OK;
}

long InitD3D( IDirect3DDevice9 **ppDevice, D3DPRESENT_PARAMETERS *pd3dPP )
{
	IDirect3D9 *pD3D;

	pD3D = Direct3DCreate9( D3D_SDK_VERSION );

	D3D = pD3D;

	// Set up the presentation parameters.
	ZeroMemory( pd3dPP, sizeof( D3DPRESENT_PARAMETERS ) );
	pd3dPP->BackBufferWidth        = 1280;
	pd3dPP->BackBufferHeight       = 720;
	pd3dPP->BackBufferFormat       = D3DFMT_X8R8G8B8;
	pd3dPP->BackBufferCount        = 1;
	pd3dPP->MultiSampleType        = D3DMULTISAMPLE_NONE;
	pd3dPP->SwapEffect             = D3DSWAPEFFECT_DISCARD;
	pd3dPP->PresentationInterval   = D3DPRESENT_INTERVAL_ONE;

	// Create the device.
	return pD3D->CreateDevice(
			0, 
			D3DDEVTYPE_HAL,
			NULL,
			D3DCREATE_HARDWARE_VERTEXPROCESSING,
			pd3dPP,
			ppDevice );


}



//--------------------------------------------------------------------------------------
// Name: UnregisterXuiClasses
// Desc: Unregisters all the scene classes.
//--------------------------------------------------------------------------------------
long CVBA360App::UnregisterXuiClasses()
{
	CVBA360Menu::Unregister();
	CRomListScene::Unregister();
	CRomList::Unregister();
	CInGameOptions::Unregister();
	CFavoritesList::Unregister();
	CFavoritesListScene::Unregister();
	return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: main
// Desc: Application entry point.
//--------------------------------------------------------------------------------------

IDirect3DDevice9 *pDevice;
D3DPRESENT_PARAMETERS d3dpp;
int IsCurrentlyInGame = false;


CVBA360App app;
CRomPathSettings romPaths;
HXUIOBJ phObj = NULL;

// In Game Scene
HXUIOBJ hScene;
HXUIOBJ hMainScene;
 
extern "C" void system_init (void);

VOID __cdecl main()
{
	// Declare an instance of the XUI framework application.
	long hr;

	MapInternalDrives();
	XSetFileCacheSize(0x10000);
	XMountUtilityDriveEx(XMOUNTUTILITYDRIVE_FORMAT0,8192, 0);
	//XboxSetDefaultValues();

	// Initialize D3D
	hr = InitD3D( &pDevice, &d3dpp );

	D3D_Device = (LPDIRECT3DDEVICE9)pDevice;
	// Initialize the application.    

	hr = app.InitShared( pDevice, &d3dpp, 
			XuiD3DXTextureLoader );

	if( FAILED( hr ) )
	{ 
		OutputDebugString( "Failed intializing application.\n" );
		return;
	}

	// Register a default typeface
	hr = app.RegisterDefaultTypeface( L"Arial Unicode MS", L"file://game:/media/vba.ttf" );
	if( FAILED( hr ) )
	{
		OutputDebugString( "Failed to register default typeface.\n" );
		return;
	}

	romPaths.Load("GAME:\\settings.xml");

	if (GetFileAttributes(romPaths.m_PreviewPath.c_str()) == -1)
	{
		// create preview dir if it doesnt exist
		wchar_t opath[MAX_PATH]; 
		swprintf_s(opath, L"%S", romPaths.m_PreviewPath.c_str());
		CreateDirectoryAnyDepth(opath);			

	}

	xStorage.Initialise();	

	// Create the notification listener to listen for XMP notifications
	//hNotificationListener = XNotifyCreateListener( XNOTIFY_XMP );


	// Load the skin file used for the scene.
	app.LoadSkin( L"file://game:/media/VBASkin.xzp#Skin\\skin.xur" );

	XuiSceneCreate( L"file://game:/media/VBASkin.xzp#Skin\\", L"MainMenu.xur", NULL, &hMainScene );

	XuiSceneNavigateFirst( app.GetRootObj(), hMainScene, XUSER_INDEX_FOCUS );

	m_Achievements = new BYTE[XACHIEVEMENT_SIZE_FULL * ACHIEVEMENT_COUNT];

	do
	{ 
		if (!IsCurrentlyInGame)
		{			 
			// Render game graphics.
			RenderGame( pDevice );

			xStorage.Update();

			// Update XUI
			app.RunFrame();

			// Render XUI
			hr = app.Render();

			// Update XUI Timers
			hr = XuiTimersRun();

			// Present the frame.
			pDevice->Present( NULL, NULL, NULL, NULL );
		}
	}while(1);

	// Free resources, unregister custom classes, and exit.
	delete(m_Achievements);
	app.Uninit();
	pDevice->Release();
}


VOID EnumerateAchievements()
{
	// Annotation:
	// Reading an achievement will cause an MU access which is slow.  Therefore,
	// reading achievements is an asynchronous operation.  Creating an event to
	// signal the completion of the operation allows you to continue other
	// processing and rendering while the write takes place.

	// Create event for asynchronous enumeration
	HANDLE hEventComplete = CreateEvent( NULL, FALSE, FALSE, NULL );

	XOVERLAPPED xov;

	ZeroMemory( &xov, sizeof(XOVERLAPPED) );
	xov.hEvent = hEventComplete;

	HANDLE hEnum;
	unsigned long cbBuffer;

	// Annotation:
	// This sample code retrieves the full details of an achievement.  The
	// dwDetailFlags (4th) parameter below can be used to control the actual
	// fields of each achievement that are filled in.  This can be helpful
	// when memory is at a premium and you don't need the image data (for
	// example).
	//
	// The two first two parameters control the title and the user 
	// making the request. The third is for the user
	// whose achievements are read. When the third parameter is set to
	// INVALID_XUID, the read is for the user's own achievements.

	// Create enumerator for the default device
	unsigned long dwStatus = XUserCreateAchievementEnumerator(
			0,                          // retrieve achievements for the current title
			0,                          // local signed-in user 0 is making the request
			INVALID_XUID ,               // achievements for the current user are to be found 
			XACHIEVEMENT_DETAILS_ALL,   // information on all achievements is to be retrieved
			0,                          // starting achievement index
			ACHIEVEMENT_COUNT,          // number of achievements to return
			&cbBuffer,             // bytes needed
			&hEnum );

	if( dwStatus != ERROR_SUCCESS )
	{
		CloseHandle( hEventComplete );

	}


	// Enumerate display names

	// Annotation
	// Once the enumerator is created it can be pumped for information.
	// The read is asynchronous so be sure to use that to your advantage
	// to keep your title responsive.

	if( XEnumerate( hEnum, m_Achievements, sizeof( m_Achievements ),
				NULL, &xov ) == ERROR_IO_PENDING )
	{
		unsigned long dwReturnCount;

		// Wait on hEventComplete handle
		if( XGetOverlappedResult( &xov, &dwReturnCount, TRUE ) == ERROR_SUCCESS )
			dwAchievementCount = dwReturnCount;
	}

	CloseHandle( hEnum );
	CloseHandle( hEventComplete );
}

void UpdatePresence(unsigned long type)
{

}
 
void DoAchievo(unsigned long AchievoID)
{
	HANDLE hEventComplete = CreateEvent( NULL, FALSE, FALSE, NULL );

	XOVERLAPPED xov;

	ZeroMemory( &xov, sizeof( XOVERLAPPED ) );
	xov.hEvent = hEventComplete;

	XUSER_ACHIEVEMENT Achievements;
	Achievements.dwUserIndex = xStorage.GetSignedInUser();
	Achievements.dwAchievementId = AchievoID;

	XUserWriteAchievements( 1, &Achievements, &xov );

	XGetOverlappedResult( &xov, NULL, TRUE );

	CloseHandle( hEventComplete );

}
 
 
