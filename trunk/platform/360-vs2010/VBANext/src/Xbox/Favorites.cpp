
#include "Favorites.h"
#include "Storage.h"
#include "Main.h"
#include "RomSettings.h"
#include "InGameOptions.h"
#include "GeneralFunctions.h"
 
extern int IsCurrentlyInGame;
extern HXUIOBJ phObj;
extern HXUIOBJ hScene; 
extern HXUIOBJ hMainScene;
extern HXUIOBJ hRomListScene;
 
extern char szRomPath[MAX_PATH];
extern GameStorage xStorage;
extern CRomPathSettings romPaths;
extern int gba_main(char *path, char *fname); 
extern IDirect3DDevice9 *pDevice;
 
 

extern CVBA360App app;
extern char InGamePreview[MAX_PATH];

extern wchar_t DeviceText[60];
std::vector<std::string> m_FavoritesListData;
std::map<std::string,std::string> favList;
extern map<string,string>::iterator romPath;
extern std::string ReplaceCharInString(const std::string & source, char charToReplace, const std::string replaceString);

 
extern int xbox_input_update(void);
 
 

#define INGAME 0
#define PAUSED 1
#define BACKTOGAME 2
#define QUIT 3
#define ROMLIST 4
 

extern void SaveConfig(void); 
extern void SaveSRAM(void);
extern void LoadSRAM(void);
 
extern t_config config;

// Handler for the XM_NOTIFY message
HRESULT CFavoritesListScene::OnNotifyPress( HXUIOBJ hObjPressed, int& bHandled )
{
		int nIndex;

        if ( hObjPressed == m_RomList )
        {
		
			nIndex = m_RomList.GetCurSel();
						 
			XuiSceneCreate( L"file://game:/media/VBASkin.xzp#Skin\\", L"InGameOptions.xur", NULL, &hScene );
			this->NavigateForward(hScene);	
 	
			gba_main((char *)favList[m_FavoritesListData[nIndex]].c_str(), (char *)m_FavoritesListData[nIndex].c_str());

			SaveConfig();
			 
			bHandled = TRUE;
			return S_OK;
			
        }
		else if ( hObjPressed == m_RemoveFavorites)
		{
			std::map <std::string,std::string>::iterator it;
			std::vector <std::string>::iterator vecIt;
			DWORD idx = 0;
			int nIndex = m_RomList.GetCurSel();
 							
			if (favList.size() > 0)
			{
				
				config.numfavs = 0;
				favList.erase(m_FavoritesListData[nIndex]);
 
				FAVORITE *favs = new FAVORITE[favList.size()];
				memset(&favs[0], 0x00, (sizeof(FAVORITE) * favList.size()));
				//rebuild/resave list
				int i = 0;
				for(it = favList.begin(); it != favList.end(); ++it)
				{			
					 
					FAVORITE f;
					  
					strcpy_s(f.filename, MAX_PATH, it->first.c_str()); 
					strcpy_s(f.path, MAX_PATH, it->second.c_str()); 	

					favs[i] = f;

					if (it!=favList.end())
					{
						i++;
						config.numfavs++;

						 
					}
				}
 

				HANDLE hEventComplete = NULL;
				XOVERLAPPED xov = {0};

				hEventComplete = CreateEvent( NULL, FALSE, FALSE, NULL );

				xov.hEvent = hEventComplete;
	
				HANDLE stream = xStorage.OpenStream("VBAFavorites.dat", L"Visual Boy Advance 360 Favorites", &xov, &hEventComplete);
	
				if (stream)
				{
					xStorage.WriteStream(stream, (char *)&favs[0], (sizeof(FAVORITE) * favList.size()));
					xStorage.CloseStream(stream, &xov);
					CloseHandle(hEventComplete);
					xStorage.WriteThumbnail("VBAFavorites.dat", "game:\\media\\saveicon.png" );

				}

				if (favs)
				{
					delete [] favs;
					favs = NULL;
				}

				// remove it from the UI
				vecIt = std::find(m_FavoritesListData.begin(), m_FavoritesListData.end(), m_FavoritesListData[nIndex].c_str());

				if (vecIt != m_FavoritesListData.end())
				{
					m_FavoritesListData.erase(vecIt);
					m_RomList.DeleteItems(nIndex, 1);
					m_RomList.SetCurSel(0);
					
				}
				
			}
 
			bHandled = TRUE;
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
    // Performs initialization tasks - retreives controls.
    //----------------------------------------------------------------------------------
long CFavoritesListScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
        // Retrieve controls for later use.
        GetChildById( L"XuiRemoveFavorites", &m_RemoveFavorites );
        GetChildById( L"XuiPlay", &m_PlayRom );
        GetChildById( L"XuiMainMenu", &m_Back );		 
		GetChildById( L"XuiRomList", &m_RomList );
		GetChildById( L"XuiRomPreview", &m_PreviewImage );	 		 
 
		GetChildById( L"XuiCurrentDeviceText", &m_DeviceText);
		GetChildById( L"XuiNextDeviceButton", &m_NextDevice);

		 
		phObj = this->m_hObj;

		m_RomList.DiscardResources(XUI_DISCARD_ALL);
		m_RomList.SetFocus();
		m_RomList.SetCurSel(0);
 
		//swprintf_s(DeviceText, L"Current Device : %S", config.rompath);
		m_DeviceText.SetText(DeviceText);

		bHandled = TRUE;
        return S_OK;
    }

CFavoritesList::CFavoritesList()
{
	BuildFavorites();

}

void BuildFavorites(void)
{
	FAVORITE fav;
	HANDLE hEventComplete = NULL;
	XOVERLAPPED xov = {0};

	config.numfavs = 0;

	hEventComplete = CreateEvent( NULL, FALSE, FALSE, NULL );

	xov.hEvent = hEventComplete;

	m_FavoritesListData.clear();
	favList.clear();

	std::vector <std::string>::iterator it;
	HANDLE stream = xStorage.OpenStreamForRead("VBAFavorites.dat", L"Visual Boy Advance 360 Configuration", &xov, &hEventComplete);
		
	if (stream)
	{
		while (xStorage.ReadStream(stream, (char *)&fav, sizeof(FAVORITE)) > 0)
		{		
			it = std::find(m_FavoritesListData.begin(), m_FavoritesListData.end(), fav.filename);

			if (it == m_FavoritesListData.end())
			{
				favList[fav.filename] = fav.path;
				m_FavoritesListData.push_back(fav.filename);
				config.numfavs++;
			}
		}
		
		xStorage.CloseStream(stream, &xov);
		CloseHandle(hEventComplete);

		romPath = romPaths.FindDevice(std::string(config.rompath));
	 
	}
	 
	std::sort(m_FavoritesListData.begin(), m_FavoritesListData.end());

}
 
long CFavoritesList::OnRescanRoms( char *szPath, char *szRom,  BOOL& bHandled )
{ 
 
	DeleteItems(0, m_FavoritesListData.size());

	BuildFavorites();
	 
	InsertItems( 0, m_FavoritesListData.size() );

	bHandled = TRUE;	
    return( S_OK );
}

long CFavoritesList::OnNotify( XUINotify *hObj, BOOL& bHandled )
{
	wchar_t previewPath[MAX_PATH];

	HXUIOBJ hPreviewImage;

	int nIndex = 0;
	switch(hObj->dwNotify)
	{
		case XN_SELCHANGED:
			 						 
			nIndex = XuiListGetCurSel( this->m_hObj, NULL );
			
			const char *fname = GetFilename((char *)romPaths.m_PreviewPath.c_str(), (char *)m_FavoritesListData[nIndex].c_str(), "png");
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

			bHandled = TRUE;

			//XuiElementPlayTimeline(hPreviewImage,20,0,25,FALSE,FALSE);
							 
			break;

	}

	return S_OK;

}
 
void InRescanFavoritesFirstFunc(XUIMessage *pMsg, InRescanFavoriteStruct* pData)
{
    XuiMessage(pMsg,XM_MESSAGE_ON_RESCAN_FAVORITES);
    _XuiMessageExtra(pMsg,(XUIMessageData*) pData, sizeof(*pData));

	
}

long CFavoritesList::OnInit(XUIMessageInit *pInitData, BOOL& bHandled)
{
 
	XUIMessage xuiMsg;
	InRescanFavoriteStruct msgData;
	InRescanFavoritesFirstFunc( &xuiMsg, &msgData);
	XuiSendMessage( m_hObj, &xuiMsg );

	bHandled = TRUE;
    return S_OK;
}

long CFavoritesList::OnGetItemCountAll(XUIMessageGetItemCount *pGetItemCountData, int& bHandled)
{
        pGetItemCountData->cItems = m_FavoritesListData.size();
        bHandled = TRUE;
        return S_OK;
}


// Gets called every frame
long CFavoritesList::OnGetSourceDataText(XUIMessageGetSourceText *pGetSourceTextData, int & bHandled)
{
		if( ( 0 == pGetSourceTextData->iData ) && ( ( pGetSourceTextData->bItemData ) ) )
		{
			LPCWSTR lpszwBuffer = MultiCharToUniChar((char *)m_FavoritesListData[pGetSourceTextData->iItem].c_str());
			pGetSourceTextData->szText = lpszwBuffer;
			bHandled = TRUE;
		}
		return S_OK;

}

 