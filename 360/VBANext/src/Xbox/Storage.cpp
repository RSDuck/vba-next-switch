#include <xtl.h>
#include <stdio.h>
#include <string.h>
#include "Storage.h"

long GameStorage::Initialise()
{
	m_dwMinUsers                   = 1;     
	m_dwMaxUsers                   = 4;     
	m_bRequireOnlineUsers          = FALSE; 
	m_dwSignInPanes                = 1;     
	m_hNotification                = NULL;  
	m_dwSignedInUserMask           = 0;     
	m_dwNumSignedInUsers           = 0;     
	m_dwOnlineUserMask             = 0;     

	m_bSystemUIShowing             = FALSE; 
	m_bNeedToShowSignInUI          = FALSE; 
	m_bMessageBoxShowing           = FALSE;
	m_bSigninUIWasShown            = FALSE;

	m_bDrawHelp = FALSE;
	m_dwSelectedIndex = 0;
	m_dwContentDataCount = 0;
	m_dwFirstSignedInUser = -1;

	// Initialize Live
	if( FAILED( XOnlineStartup() ) )
		return E_FAIL;

	m_hNotification = XNotifyCreateListener( XNOTIFY_SYSTEM  | XNOTIFY_LIVE );
	if( m_hNotification == NULL )
		return E_FAIL;

	QuerySigninStatus();

	// Start sample by showing the signin UI for one user
	m_bFirstSignin = TRUE;
	ShowSignInUI();

	// Clear message string
	m_szMessage[0] = L'\0';

	m_bDeviceUIActive = FALSE;
	m_DeviceID = 0;

	return S_OK;

}

VOID GameStorage::QuerySigninStatus()
{
    m_dwSignedInUserMask = 0;
    m_dwOnlineUserMask = 0;

    // Count the signed-in users
    m_dwNumSignedInUsers = 0;
    m_dwFirstSignedInUser = ( DWORD )-1;

    for( UINT nUser = 0;
         nUser < XUSER_MAX_COUNT;
         nUser++ )
    {
        XUSER_SIGNIN_STATE State = XUserGetSigninState( nUser );

        if( State != eXUserSigninState_NotSignedIn )
	{
		// Check whether the user is online
		int bUserOnline = State == eXUserSigninState_SignedInToLive;

		m_dwOnlineUserMask |= bUserOnline << nUser;

		// If we want Online users only, only count signed-in users
		if( !m_bRequireOnlineUsers || bUserOnline )
		{
			m_dwSignedInUserMask |= ( 1 << nUser );

			if( m_dwFirstSignedInUser == (unsigned long)-1 )
				m_dwFirstSignedInUser = nUser;

			++m_dwNumSignedInUsers;
		}
	}
    }

    // check to see if we need to invoke the signin UI
    m_bNeedToShowSignInUI = !AreUsersSignedIn();

}

unsigned long GameStorage::SignInUpdate()
{
	unsigned long dwRet = 0;

	// Check for system notifications
	unsigned long dwNotificationID;
	ULONG_PTR ulParam;

	if( XNotifyGetNext( m_hNotification, 0, &dwNotificationID, &ulParam ) )
	{
		switch( dwNotificationID )
		{
			case XN_SYS_SIGNINCHANGED:
				dwRet |= SIGNIN_USERS_CHANGED;

				// Query who is signed in
				QuerySigninStatus();
				break;

			case XN_SYS_UI:
				dwRet |= SYSTEM_UI_CHANGED;
				m_bSystemUIShowing = static_cast<int>( ulParam );

				// check to see if we need to invoke the signin UI
				m_bNeedToShowSignInUI = !AreUsersSignedIn();
				break;

			case XN_LIVE_CONNECTIONCHANGED:
				dwRet |= CONNECTION_CHANGED;
				break;

		}
	}

	// If there are not enough or too many profiles signed in, display an 
	// error message box prompting the user to either sign in again or exit the sample
	if( !m_bMessageBoxShowing && !m_bSystemUIShowing && m_bSigninUIWasShown && !AreUsersSignedIn() )
	{
		unsigned long dwResult;

		ZeroMemory( &m_Overlapped, sizeof( XOVERLAPPED ) );

		WCHAR strMessage[512];
		swprintf_s( strMessage, L"Incorrect number of profiles signed in. You must sign in at least %d"
				L" and at most %d profiles. Currently there are %d profiles signed in.",
				m_dwMinUsers, m_dwMaxUsers, m_dwNumSignedInUsers );

		dwResult = XShowMessageBoxUI( XUSER_INDEX_ANY,
				L"Signin Error",   // Message box title
				strMessage,                 // Message
				ARRAYSIZE( m_pwstrButtons ),// Number of buttons
				m_pwstrButtons,             // Button captions
				0,                          // Button that gets focus
				XMB_ERRORICON,              // Icon to display
				&m_MessageBoxResult,        // Button pressed result
				&m_Overlapped );


		m_bSystemUIShowing = TRUE;
		m_bMessageBoxShowing = TRUE;
	}

	// Wait until the message box is discarded, then either exit or show the signin UI again
	if( m_bMessageBoxShowing && XHasOverlappedIoCompleted( &m_Overlapped ) )
	{
		m_bMessageBoxShowing = FALSE;

		if( XGetOverlappedResult( &m_Overlapped, NULL, TRUE ) == ERROR_SUCCESS )
		{
			switch( m_MessageBoxResult.dwButtonPressed )
			{
				case 0:     // Reboot to the launcher
					XLaunchNewImage( "", 0 );
					break;

				case 1:     // Show the signin UI again
					ShowSignInUI();
					m_bSigninUIWasShown = FALSE;
					break;
			}
		}
	}

	// Check to see if we need to invoke the signin UI
	if( !m_bMessageBoxShowing && m_bNeedToShowSignInUI && !m_bSystemUIShowing )
	{
		m_bNeedToShowSignInUI = FALSE;

		unsigned long ret = XShowSigninUI(m_dwSignInPanes, m_bRequireOnlineUsers ? XSSUI_FLAGS_LOCALSIGNINONLY : 0 );

		if( ret != ERROR_SUCCESS )
		{

		}
		else
		{
			m_bSystemUIShowing = TRUE;
			m_bSigninUIWasShown = TRUE;
		}
	}

	return dwRet;
}

long GameStorage::Update()
{

	// Update login
	SignInUpdate();

	// If we're not signed in, wait until we are
	if(  !AreUsersSignedIn() || IsSystemUIShowing())
		return S_OK;

	unsigned long dwNotificationId;
	ULONG ulParam;
	if( XNotifyGetNext( m_hNotification, 0, &dwNotificationId, &ulParam ) )
	{
		switch( dwNotificationId )
		{
			case XN_SYS_STORAGEDEVICESCHANGED:
				swprintf_s( m_szMessage, L"Devices have been removed or inserted." );
				ShowDeviceUI();
				break;
		}
	}


	// Show the device selector UI the first time the user has signed in
	if( m_bFirstSignin )
	{
		m_bFirstSignin = FALSE;
		ShowDeviceUI();
	}

	// Enumerate content names once device UI has been closed
	if( m_bDeviceUIActive )
	{
		if( XHasOverlappedIoCompleted( &m_Overlapped ) )
		{
			if( m_DeviceID == XCONTENTDEVICE_ANY )
			{
				ShowDeviceUI();
			}
			else
			{
				//EnumerateContentNames();
				m_bDeviceUIActive = FALSE;
			}
		}
	}


	return S_OK;
}

VOID GameStorage::ShowDeviceUI()
{
	unsigned long dwRet;

	ZeroMemory( &m_Overlapped, sizeof( XOVERLAPPED ) );
	m_DeviceID = XCONTENTDEVICE_ANY;

	ULARGE_INTEGER iBytesRequested = {0};

	dwRet = XShowDeviceSelectorUI( GetSignedInUser(), // User to receive input from
			XCONTENTTYPE_SAVEDGAME,   // List only save game devices
			0,                       // No special flags
			iBytesRequested,         // Size of the device data struct
			&m_DeviceID,            // Return selected device information
			&m_Overlapped );


	m_bDeviceUIActive = TRUE;
}


//--------------------------------------------------------------------------------------
// Name: WriteSaveGame
//  
//--------------------------------------------------------------------------------------
unsigned long GameStorage::WriteSaveGame( CONST CHAR *szFileName, CONST WCHAR* szDisplayName, CHAR *szBuffer, unsigned long bytesToWrite )
{
	// Create event for asynchronous writing
	HANDLE hEventComplete = CreateEvent( NULL, FALSE, FALSE, NULL );


	XOVERLAPPED xov = {0};
	xov.hEvent = hEventComplete;

	XCONTENT_DATA contentData = {0};
	strcpy_s( contentData.szFileName, szFileName );
	wcscpy_s( contentData.szDisplayName, szDisplayName );
	contentData.dwContentType = XCONTENTTYPE_SAVEDGAME;
	contentData.DeviceID = m_DeviceID;

	// Mount the device associated with the display name for writing
	unsigned long dwErr = XContentCreate( GetSignedInUser(), g_szSaveRoot, &contentData, XCONTENTFLAG_CREATEALWAYS, NULL, NULL, &xov );

	if( dwErr != ERROR_IO_PENDING )
	{
		CloseHandle( hEventComplete );

		return dwErr;
	}

	// Wait on hEventComplete handle
	if( XGetOverlappedResult( &xov, NULL, TRUE ) == ERROR_SUCCESS )
	{
		HANDLE hFile = CreateFile( g_szSaveGame, GENERIC_WRITE, 0,
				NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

		if( hFile != INVALID_HANDLE_VALUE )
		{
			// Write dummy data to the file

			unsigned long dwWritten;

			if( WriteFile( hFile, ( VOID* )szBuffer, bytesToWrite, &dwWritten, NULL ) == 0 )
			{
				CloseHandle( hFile );
				XContentClose( g_szSaveRoot, &xov );
				XGetOverlappedResult( &xov, NULL, TRUE );
				CloseHandle( hEventComplete );


				return GetLastError();
			}

			CloseHandle( hFile );
		}
		else
			return GetLastError();
	}

	XContentClose( g_szSaveRoot, &xov );

	// Wait for XCloseContent to complete
	XGetOverlappedResult( &xov, NULL, TRUE );

	CloseHandle( hEventComplete );

	return ERROR_SUCCESS;
}


HANDLE GameStorage::OpenStream(CONST CHAR* szFileName, CONST WCHAR* szDisplayName, XOVERLAPPED *xov, HANDLE *hEventComplete)
{
	HANDLE hFile = NULL;

	XCONTENT_DATA contentData = {0};
	strcpy_s( contentData.szFileName, szFileName );
	wcscpy_s( contentData.szDisplayName, szDisplayName );
	contentData.dwContentType = XCONTENTTYPE_SAVEDGAME;
	contentData.DeviceID = m_DeviceID;

	// Mount the device associated with the display name for writing
	unsigned long dwErr = XContentCreate( GetSignedInUser(), g_szSaveRoot, &contentData,
			XCONTENTFLAG_CREATEALWAYS, NULL, NULL, xov );
	if( dwErr != ERROR_IO_PENDING )
	{
		CloseHandle( hEventComplete );

		return NULL;
	}

	// Wait on hEventComplete handle
	if( XGetOverlappedResult( xov, NULL, TRUE ) == ERROR_SUCCESS )
		hFile = CreateFile( g_szSaveGame, GENERIC_WRITE , 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

	return hFile;

}


HANDLE GameStorage::OpenStreamForNewAppend(CONST CHAR* szFileName, CONST WCHAR* szDisplayName, XOVERLAPPED *xov, HANDLE *hEventComplete, DWORD size, DWORD index)
{

	HANDLE hFile = NULL;

	XCONTENT_DATA contentData = {0};
	strcpy_s( contentData.szFileName, szFileName );
	wcscpy_s( contentData.szDisplayName, szDisplayName );
	contentData.dwContentType = XCONTENTTYPE_SAVEDGAME;
	contentData.DeviceID = m_DeviceID;

	// Mount the device associated with the display name for writing
	DWORD dwErr = XContentCreate( GetSignedInUser(), g_szSaveRoot, &contentData,
			XCONTENTFLAG_OPENALWAYS  , NULL, NULL, xov );
	if( dwErr != ERROR_IO_PENDING )
	{
		CloseHandle( hEventComplete );

		return NULL;
	}

	// Wait on hEventComplete handle
	if( XGetOverlappedResult( xov, NULL, TRUE ) == ERROR_SUCCESS )
	{
		hFile = CreateFile( g_szSaveGame, GENERIC_WRITE , 0,
				NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

		SetFilePointer(hFile, size * index , NULL, FILE_CURRENT);


	}

	return hFile;

}



HANDLE GameStorage::OpenStreamForRead(CONST CHAR* szFileName, CONST WCHAR* szDisplayName, XOVERLAPPED *xov, HANDLE *hEventComplete)
{
	HANDLE hFile = NULL;

	XCONTENT_DATA contentData = {0};
	strcpy_s( contentData.szFileName, szFileName );
	wcscpy_s( contentData.szDisplayName, szDisplayName );
	contentData.dwContentType = XCONTENTTYPE_SAVEDGAME;
	contentData.DeviceID = m_DeviceID;

	// Mount the device for reading
	DWORD dwErr = XContentCreate( GetSignedInUser(), g_szSaveRoot, &contentData,
			XCONTENTFLAG_OPENEXISTING, NULL, NULL, xov );
	if( dwErr != ERROR_IO_PENDING )
	{
		CloseHandle( hEventComplete );

		return NULL;
	}

	// Wait on hEventComplete handle
	if( XGetOverlappedResult( xov, NULL, TRUE ) == ERROR_SUCCESS )
		hFile = CreateFile( g_szSaveGame, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );

	return hFile;

}

unsigned long GameStorage::WriteStream(HANDLE hFile, char *szBuffer, unsigned long bytesToWrite )
{
	unsigned long dwWritten = 0;

	if( WriteFile( hFile, ( VOID* )szBuffer, bytesToWrite, &dwWritten, NULL ) == 0 )
		return GetLastError();

	return dwWritten;

}

unsigned long GameStorage::ReadStream(HANDLE hFile, char *szBuffer, unsigned long bytesToRead)
{
	unsigned long dwRead = 0;

	if (ReadFile(hFile, (VOID *)szBuffer, bytesToRead, &dwRead, NULL) == 0)
		return GetLastError();

	return dwRead;
}
 

VOID  GameStorage::CloseStream(HANDLE hFile, XOVERLAPPED *xov)
{
	CloseHandle( hFile );	
	XContentClose( g_szSaveRoot, xov );     
	XGetOverlappedResult( xov, NULL, TRUE );
}

VOID GameStorage::EnumerateContentNames()
{
	HANDLE hEnum;
	unsigned long cbBuffer;

	// Create enumerator for the default device
	unsigned long dwRet;
	dwRet = XContentCreateEnumerator( GetSignedInUser(), // Access data associated with this user
			m_DeviceID,         // Pass in selected device
			XCONTENTTYPE_SAVEDGAME,
			0,                     // No special flags
			ARRAYSIZE( m_ContentData ),
			&cbBuffer,
			&hEnum );

	// Enumerate display names
	m_dwContentDataCount = 0;

	unsigned long dwReturnCount;
	if( XEnumerate( hEnum, m_ContentData, sizeof( m_ContentData ), &dwReturnCount, NULL ) == ERROR_SUCCESS )
		m_dwContentDataCount = dwReturnCount;

	CloseHandle( hEnum );
}


//--------------------------------------------------------------------------------------
// Name: ReadSaveGame
// Desc: Read the previously saved dummy game file.
//--------------------------------------------------------------------------------------
unsigned long GameStorage::ReadSaveGame( XCONTENT_DATA* ContentData, char *szBuffer, unsigned long bytesToRead, unsigned long *bytesRead )
{
	// Create event for asynchronous reading
	HANDLE hEventComplete = CreateEvent( NULL, FALSE, FALSE, NULL );


	XOVERLAPPED xov = {0};
	xov.hEvent = hEventComplete;

	// Mount the device for reading

	unsigned long dwErr = XContentCreate( GetSignedInUser(), g_szSaveRoot, ContentData, XCONTENTFLAG_OPENEXISTING, NULL, NULL, &xov );

	if( dwErr != ERROR_IO_PENDING )
	{
		CloseHandle( hEventComplete );

		return dwErr;
	}

	dwErr = XGetOverlappedResult( &xov, NULL, TRUE );
	if( dwErr == ERROR_SUCCESS )
	{
		HANDLE hFile = CreateFile( g_szSaveGame, GENERIC_READ,
				FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );

		if( hFile != INVALID_HANDLE_VALUE )
		{
			//
			// Read some data from the file
			// Note, that when using XCONTENTFLAG_OPENEXISTING in XContentCreate, only
			// reading is allowed from the file, writing will have no effect.
			//

			unsigned long dwRead;

			if( ReadFile( hFile, ( VOID* )szBuffer, bytesToRead, &dwRead, NULL ) == 0 )
			{
				CloseHandle( hFile );
				XContentClose( g_szSaveRoot, &xov );
				XGetOverlappedResult( &xov, NULL, TRUE );
				CloseHandle( hEventComplete );

				bytesRead = &dwRead;
				return GetLastError();
			}

			CloseHandle( hFile );
		}
		else
			return GetLastError();
	}
	else
	{
		XContentClose( g_szSaveRoot, &xov );
		XGetOverlappedResult( &xov, NULL, TRUE );
		CloseHandle( hEventComplete );

		return dwErr;
	}

	XContentClose( g_szSaveRoot, &xov );

	// Wait for XCloseContent to complete
	XGetOverlappedResult( &xov, NULL, TRUE );

	CloseHandle( hEventComplete );

	return ERROR_SUCCESS;
}


//--------------------------------------------------------------------------------------
// Name: WriteThumbnail
// Desc: Write a XContent thumbnail image. Returns TRUE if successful.
//--------------------------------------------------------------------------------------
int GameStorage::WriteThumbnail( CONST CHAR* szFileName, char * szThumbnailFile )
{
	XCONTENT_DATA contentData = {0};

	strcpy_s( contentData.szFileName, szFileName );
	contentData.dwContentType = XCONTENTTYPE_SAVEDGAME;
	contentData.DeviceID = m_DeviceID;

	// Read the thumbnail image file
	HANDLE hFile = CreateFile( szThumbnailFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );

	if( hFile == INVALID_HANDLE_VALUE )
		return FALSE;

	unsigned long dwThumbnailBytes = GetFileSize( hFile, NULL );

	BYTE* pbThumbnailImage = new BYTE[dwThumbnailBytes];
	if( pbThumbnailImage == NULL )
	{
		CloseHandle( hFile );

		return FALSE;
	}

	if( ReadFile( hFile, pbThumbnailImage, dwThumbnailBytes, &dwThumbnailBytes, NULL ) == 0 )
	{
		CloseHandle( hFile );
		delete [] pbThumbnailImage;

		return FALSE;
	}

	if( XContentSetThumbnail( GetSignedInUser(), &contentData, pbThumbnailImage, dwThumbnailBytes, NULL ) != ERROR_SUCCESS )
	{
		CloseHandle( hFile );
		delete [] pbThumbnailImage;

		return FALSE;
	}

	CloseHandle( hFile );
	delete [] pbThumbnailImage;

	return TRUE;
}
