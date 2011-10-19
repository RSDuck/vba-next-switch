#include<xtl.h>
#include<d3d9.h>
#include<xam.h>

#pragma once
#ifndef STORAGE_C
#define STORAGE_C

static const unsigned long  MAX_DATA_COUNT = 10;
//--------------------------------------------------------------------------------------
// Save root name to use. This can be any valid directory name.
//--------------------------------------------------------------------------------------
static char g_szSaveRoot[] = "save";

//--------------------------------------------------------------------------------------
// Dummy file name to use for the save game.
//--------------------------------------------------------------------------------------
static char g_szSaveGame[] = "save:\\savegame.txt";

static char g_szThumbnailImage[] = "game:\\saveicon.png";

static unsigned long  m_dwMinUsers                   = 1;     
static unsigned long  m_dwMaxUsers                   = 1;     
static int   m_bRequireOnlineUsers          = FALSE; 
static unsigned long  m_dwSignInPanes                = 1;     
static HANDLE m_hNotification                = NULL;  
static unsigned long  m_dwSignedInUserMask           = 0;     
static unsigned long  m_dwNumSignedInUsers           = 0;     
static unsigned long  m_dwOnlineUserMask             = 0;     

static int   m_bSystemUIShowing             = FALSE; 
static int   m_bNeedToShowSignInUI          = FALSE; 
static int   m_bMessageBoxShowing           = FALSE;
static int   m_bSigninUIWasShown            = FALSE;
static XOVERLAPPED m_Overlapped              = {0};
static LPCWSTR m_pwstrButtons[2]             = { L"Exit", L"VBA360 Sign In" };
static MESSAGEBOX_RESULT m_MessageBoxResult  = {0};

// Flags that can be returned from Update()
static enum SIGNIN_UPDATE_FLAGS
{
    SIGNIN_USERS_CHANGED    = 0x00000001,
    SYSTEM_UI_CHANGED       = 0x00000002,
    CONNECTION_CHANGED      = 0x00000004
};

class GameStorage
{

	public:
	long Initialise();
	void ShowDeviceUI();

	int m_bDrawHelp;
	XCONTENT_DATA       m_ContentData[MAX_DATA_COUNT];  // Data containing display names
	LPDIRECT3DTEXTURE9  m_ThumbnailTextures[MAX_DATA_COUNT];
	unsigned long m_dwContentDataCount;           // Number of data elements
	unsigned long m_dwSelectedIndex;              // Selected element in UI
	WCHAR               m_szMessage[128];               // Message to be displayed in UI
	XOVERLAPPED m_Overlapped;                   // Overlapped object for device selector UI
	XCONTENTDEVICEID m_DeviceID;                   // Device identifier returned by device selector UI
	int m_bFirstSignin;                 // Flag indicating first signin
	int m_bDeviceUIActive;              // Is the device UI showing
	HANDLE m_hNotification;
	unsigned long  m_dwFirstSignedInUser;
	VOID                EnumerateContentNames();
	unsigned long WriteSaveGame( CONST char * szFileName, CONST WCHAR* szDisplayName, char *szBuffer, unsigned long bytesToWrite );	
	unsigned long ReadSaveGame( XCONTENT_DATA* ContentData, char *szBuffer, unsigned long bytesToRead, unsigned long *bytesRead );
	int                ReadThumbnail( XCONTENT_DATA* pContentData, LPDIRECT3DTEXTURE9* pThumbnailTexture );     
	int                TransferContent( XCONTENT_DATA* pContentData );
	int                DeleteContent( XCONTENT_DATA* pContentData );
	VOID QuerySigninStatus();
	int WriteThumbnail( CONST char * szFileName, char * szThumbnailFile );
	unsigned long SignInUpdate();
	long Update();

	HANDLE OpenStream(CONST CHAR* szFileName, CONST WCHAR* szDisplayName, XOVERLAPPED *xov, HANDLE *hEventComplete);
	HANDLE OpenStreamForRead(CONST CHAR* szFileName, CONST WCHAR* szDisplayName, XOVERLAPPED *xov, HANDLE *hEventComplete);
	HANDLE OpenStreamForNewAppend(CONST CHAR* szFileName, CONST WCHAR* szDisplayName, XOVERLAPPED *xov, HANDLE *hEventComplete, unsigned long size, unsigned long index);
	unsigned long WriteStream(HANDLE hFile, char *szBuffer, unsigned long bytesToWrite);
	unsigned long ReadStream(HANDLE hFile, char *szBuffer, unsigned long bytesToRead);
	VOID CloseStream(HANDLE hFile, XOVERLAPPED *xov);

	// Check users that are signed in
	static unsigned long GetSignedInUserCount()
	{
		return m_dwNumSignedInUsers;
	}
	static unsigned long GetSignedInUserMask()
	{
		return m_dwSignedInUserMask;
	}
	static int IsUserSignedIn( unsigned long dwController )
	{
		return ( m_dwSignedInUserMask & ( 1 << dwController ) ) != 0;
	}

	static int AreUsersSignedIn()
	{
		return ( m_dwNumSignedInUsers >= m_dwMinUsers ) &&
			( m_dwNumSignedInUsers <= m_dwMaxUsers );
	}

	// Get the first signed-in user
	unsigned long GetSignedInUser()
	{
		return m_dwFirstSignedInUser;
	}

	// Check users that are signed into live
	static unsigned long GetOnlineUserMask()
	{
		return m_dwOnlineUserMask;
	}
	static int     IsUserOnline( unsigned long dwController )
	{
		return ( m_dwOnlineUserMask & ( 1 << dwController ) ) != 0;
	}

	// Check the presence of system UI
	static int  IsSystemUIShowing()
	{
		return m_bSystemUIShowing || m_bNeedToShowSignInUI;
	}

	// Function to reinvoke signin UI
	static VOID     ShowSignInUI()
	{
		m_bNeedToShowSignInUI = TRUE;
	}
};


#endif
