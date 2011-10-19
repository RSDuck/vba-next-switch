 
#include "RomList.h"
#include "Storage.h"
#include "Main.h"
 
#include "RomSettings.h"
#include "InGameOptions.h"
#include "GeneralFunctions.h"
#include "Favorites.h"
 
extern int IsCurrentlyInGame;
extern HXUIOBJ phObj;
extern HXUIOBJ hScene; 
extern HXUIOBJ hMainScene;
extern HXUIOBJ hRomListScene;
char szRoms[MAX_PATH]; 
char szRomPath[MAX_PATH];
extern GameStorage xStorage;
extern CRomPathSettings romPaths;

extern int gba_main(char *path, char *fname);
extern t_config config;

extern CVBA360App app;
char InGamePreview[MAX_PATH];

wchar_t DeviceText[60];
std::vector<std::string> m_ListData;
map<string,string>::iterator romPath;
std::string ReplaceCharInString(const std::string & source, char charToReplace, const std::string replaceString);
 
extern std::map<std::string,std::string> favList;
int xbox_input_update(void);
 
 
void SaveConfig(void)
{
	HANDLE hEventComplete = NULL;
	XOVERLAPPED xov = {0};

	strcpy(config.rompath, romPath->second.c_str());
	hEventComplete = CreateEvent( NULL, FALSE, FALSE, NULL );

	xov.hEvent = hEventComplete;

	HANDLE stream = xStorage.OpenStream("VBAConfig", L"Visual Boy Advance 360 Configuration", &xov, &hEventComplete);

	if (stream)
	{
		xStorage.WriteStream(stream, (char *)&config, sizeof(config));
		xStorage.CloseStream(stream, &xov);
		CloseHandle(hEventComplete);
		xStorage.WriteThumbnail("VBAConfig", "game:\\media\\saveicon.png" );
	}

}


void SaveSRAM(void)
{
 
	wchar_t romName[50];
	char srmName[MAX_PATH];
	//sprintf_s(srmName, GetFilename("", rom_filename, "srm"));
	//swprintf_s(romName, L"%S", GetFilename("", rom_filename, "srm"));
 
	//xStorage.WriteSaveGame(srmName,romName, (char *) sram.sram, 0x10000);
	//xStorage.WriteThumbnail(srmName, "game:\\media\\saveicon.png" );
		 

}


void LoadSRAM(void)
{

	int len = 0;

	XCONTENT_DATA contentData;

	wchar_t romName[50];
	char srmName[MAX_PATH];
	//sprintf_s(srmName, GetFilename("", rom_filename, "srm"));
	//swprintf_s(romName, L"%S", GetFilename("", rom_filename, "srm"));

	contentData.DeviceID = xStorage.m_DeviceID;
	contentData.dwContentType = XCONTENTTYPE_SAVEDGAME;
	swprintf_s(contentData.szDisplayName,romName);
	strcpy_s(contentData.szFileName,srmName);

//	xStorage.ReadSaveGame(&contentData, (char *)sram.sram, 0x10000, (unsigned long *)len);

}

void InRescanRomsFirstFunc(XUIMessage *pMsg, InRescanRomsStruct* pData, char *szPath)
{
    XuiMessage(pMsg,XM_MESSAGE_ON_RESCAN_ROMS);
    _XuiMessageExtra(pMsg,(XUIMessageData*) pData, sizeof(*pData));

	
}

void AddToFavorites(char *path, char*file, unsigned long numFavs)
{
	FAVORITE fav;

	strcpy_s(fav.path, path);
	strcpy_s(fav.filename, file);

	HANDLE hEventComplete = NULL;
	XOVERLAPPED xov = {0};

	hEventComplete = CreateEvent( NULL, FALSE, FALSE, NULL );

	xov.hEvent = hEventComplete;
	
	HANDLE stream = xStorage.OpenStreamForNewAppend("VBAFavorites.dat", L"Visual Boy Advance 360 Favorites", &xov, &hEventComplete, sizeof (FAVORITE), numFavs);
	
	if (stream)
	{
		xStorage.WriteStream(stream, (char *)&fav, sizeof(fav));
		xStorage.CloseStream(stream, &xov);
		CloseHandle(hEventComplete);
		xStorage.WriteThumbnail("VBAFavorites.dat", "game:\\media\\saveicon.png" );

	}

	config.numfavs++;
}

void AddAllToFavorites(char *buff, unsigned long size) {}

// Handler for the XM_NOTIFY message
long CRomListScene::OnNotifyPress( HXUIOBJ hObjPressed, int& bHandled )
{
	int nIndex;

	if ( hObjPressed == m_RomList )
	{

		nIndex = m_RomList.GetCurSel();

		//XuiSceneCreate( L"file://game:/media/VBASkin.xzp#Skin\\", L"InGameOptions.xur", NULL, &hScene );
		//this->NavigateForward(hScene);	

		strcpy(InGamePreview, (char *)GetFilename((char *)romPaths.m_PreviewPath.c_str(), (char *)m_ListData[nIndex].c_str(), "png"));

		gba_main((char *)romPath->second.c_str(), (char *)m_ListData[nIndex].c_str());

		SaveConfig();

		bHandled = TRUE;
		return S_OK;

	}
	else if (hObjPressed == m_AddToFavorites)
	{
		const WCHAR * button_text = L"OK";

		if (m_ListData.size() > 0)
		{
			unsigned long nIndex = m_RomList.GetCurSel();

			map<string, string>::const_iterator it = favList.find(m_ListData[nIndex]);

			if ( it == favList.end() || favList.size() == 0 )
			{
				AddToFavorites((char *)romPath->second.c_str(), (char *)m_ListData[nIndex].c_str(), config.numfavs);				
				ShowMessageBoxEx(NULL,NULL,L"Favorites", L"Game added to Favorites", 1, (LPCWSTR*)&button_text,NULL,  XUI_MB_CENTER_ON_PARENT, NULL);
				BuildFavorites();
			}
			else
			{
				ShowMessageBoxEx(NULL,NULL,L"Favorites", L"Game already in your Favorites", 1, (LPCWSTR*)&button_text,NULL,  XUI_MB_CENTER_ON_PARENT, NULL);
			}
		}

	}
	else if (hObjPressed == m_NextDevice)
	{
		XuiImageElementSetImagePath(m_PreviewImage.m_hObj, L"");

		romPath++;

		if (romPath == romPaths.GetDeviceMapEnd())
		{
			romPath = romPaths.GetDeviceMapBegin();
		}


		XUIMessage xuiMsg;
		InRescanRomsStruct msgData;
		InRescanRomsFirstFunc( &xuiMsg, &msgData, (char *)romPath->second.c_str() );
		XuiSendMessage( m_RomList.m_hObj, &xuiMsg );

		strcpy((char *)szRomPath, romPath->first.c_str());
		swprintf_s(DeviceText, L"Current Device : %S", szRomPath);
		m_DeviceText.SetText(DeviceText);			
		m_RomList.SetCurSel(0);			 


		SaveConfig();

		return S_OK;

	}

	if( XuiControlIsBackButton( hObjPressed ) )
	{
		this->NavigateBack();

	}



	bHandled = TRUE;
	return S_OK;
}


//----------------------------------------------------------------------------------
// Performs initialization tasks - retrieves controls.
//----------------------------------------------------------------------------------
long CRomListScene::OnInit( XUIMessageInit* pInitData, int& bHandled )
{

	// Retrieve controls for later use.
	GetChildById( L"XuiAddToFavorites", &m_AddToFavorites );
	GetChildById( L"XuiPlay", &m_PlayRom );
	GetChildById( L"XuiMainMenu", &m_Back );		 
	GetChildById( L"XuiRomList", &m_RomList );
	GetChildById( L"XuiRomPreview", &m_PreviewImage );	 		 
	//GetChildById( L"XuiBackVideoRoms", &m_BackVideo );
	GetChildById( L"XuiCurrentDeviceText", &m_DeviceText);
	GetChildById( L"XuiNextDeviceButton", &m_NextDevice);

	//m_BackVideo.SetVolume(-96.0);		 
	//m_BackVideo.Play( L"file://game:/media/Genesis360.xzp#Skin\\GenesisVideo.wmv" );

	phObj = this->m_hObj;

	m_RomList.DiscardResources(XUI_DISCARD_ALL);
	m_RomList.SetFocus();
	m_RomList.SetCurSel(0);

	swprintf_s(DeviceText, L"Current Device : %S", szRomPath);
	m_DeviceText.SetText(DeviceText);

	BuildFavorites(); 

	bHandled = TRUE;
	return S_OK;
}

CRomList::CRomList()
{
	romPath = romPaths.GetDeviceMapBegin();
	 
 
	HANDLE hEventComplete = NULL;
	XOVERLAPPED xov = {0};

	hEventComplete = CreateEvent( NULL, FALSE, FALSE, NULL );

	xov.hEvent = hEventComplete;
 

	HANDLE stream = xStorage.OpenStreamForRead("VBAConfig", L"Visual Boy Advance 360 Configuration", &xov, &hEventComplete);
		
	if (stream)
	{
		xStorage.ReadStream(stream, (char *)&config, sizeof(config));
		xStorage.CloseStream(stream, &xov);
		CloseHandle(hEventComplete);

		
		if (strlen(config.rompath) == 0)
		{
			strcpy_s(config.rompath, "GAME:\\ROMS\\");
		}

		romPath = romPaths.FindDevice(std::string(config.rompath));
 
	}

	strcpy((char *)config.rompath, romPath->second.c_str());
	strcpy((char *)szRomPath, romPath->first.c_str());	
	swprintf_s(DeviceText, L"Current Device : %S", szRomPath);

}
 

long CRomList::OnNotify( XUINotify *hObj, int& bHandled )
{
	wchar_t previewPath[MAX_PATH];


	HXUIOBJ hPreviewImage;

	int nIndex = 0;
	switch(hObj->dwNotify)
	{
		case XN_SELCHANGED:

			nIndex = XuiListGetCurSel( this->m_hObj, NULL );

			const char *fname = GetFilename((char *)romPaths.m_PreviewPath.c_str(), (char *)m_ListData[nIndex].c_str(), "png");
			XuiElementGetChildById( phObj, 
					L"XuiRomPreview", &hPreviewImage );

			if (GetFileAttributes(fname) != -1)
			{
				string previewName(fname);

				swprintf_s(previewPath,L"file://%S", ReplaceCharInString(romPaths.m_PreviewPath, '\\', "/").c_str());
				XuiElementDiscardResources(hPreviewImage, XUI_DISCARD_ALL);
				XuiElementSetBasePath(hPreviewImage, previewPath);

				previewName = ReplaceCharInString(previewName, '\\',"/");
				swprintf_s(previewPath,L"file://%S", previewName.c_str());
				XuiImageElementSetImagePath(hPreviewImage, previewPath);
			}
			else
			{
				XuiElementDiscardResources(hPreviewImage, XUI_DISCARD_ALL);
				XuiImageElementSetImagePath(hPreviewImage, L"nintendo_blue.jpg");
			}

			break;

	}

	return S_OK;

}
 

long CRomList::OnRescanRoms( char *szPath,  int & bHandled )
{ 
	
	DeleteItems(0, m_ListData.size());
	strcpy((char *)szRoms, romPath->second.c_str());
	strcat(szRoms, "*.*");
  
	m_ListData.clear();

	HANDLE hFind;	
	WIN32_FIND_DATAA oFindData;

	hFind = FindFirstFile(szRoms, &oFindData);

	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{		
			
			m_ListData.push_back(_strlwr(oFindData.cFileName));
			 

		} while (FindNextFile(hFind, &oFindData));

	
	}

	std::sort(m_ListData.begin(), m_ListData.end());
	 
	InsertItems( 0, m_ListData.size() );

	bHandled = TRUE;	
    return( S_OK );
}

long CRomList::OnInit(XUIMessageInit *pInitData, int & bHandled)
{
 
	XUIMessage xuiMsg;
	InRescanRomsStruct msgData;
	InRescanRomsFirstFunc( &xuiMsg, &msgData, (char *)romPath->second.c_str() );
	XuiSendMessage( m_hObj, &xuiMsg );

	bHandled = TRUE;
    return S_OK;
}

long CRomList::OnGetItemCountAll(
        XUIMessageGetItemCount *pGetItemCountData, 
        BOOL& bHandled)
    {
        pGetItemCountData->cItems = m_ListData.size();
        bHandled = TRUE;
        return S_OK;
    }


// Gets called every frame
long CRomList::OnGetSourceDataText(
    XUIMessageGetSourceText *pGetSourceTextData, 
    int & bHandled)
{
	
    
    if( ( 0 == pGetSourceTextData->iData ) && ( ( pGetSourceTextData->bItemData ) ) ) {

			LPCWSTR lpszwBuffer = MultiCharToUniChar((char *)m_ListData[pGetSourceTextData->iItem].c_str());

            pGetSourceTextData->szText = lpszwBuffer;

            bHandled = TRUE;
        }
        return S_OK;

}


std::string ReplaceCharInString(  
    const std::string & source, 
    char charToReplace, 
    const std::string replaceString 
    ) 
{ 
    std::string result; 
 
    // For each character in source string: 
    const char * pch = source.c_str(); 
    while ( *pch != '\0' ) 
    { 
        // Found character to be replaced? 
        if ( *pch == charToReplace ) 
        { 
            result += replaceString; 
        } 
        else 
        { 
            // Just copy original character 
            result += (*pch); 
        } 
 
        // Move to next character 
        ++pch; 
    } 
 
    return result; 
} 

