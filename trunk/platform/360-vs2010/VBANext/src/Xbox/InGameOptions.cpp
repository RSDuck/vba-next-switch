#include "Main.h"
#include "InGameOptions.h"
#include "Storage.h"
#include "RomList.h"
#include "ScreenShot.h"
#include "System.h"
#include "../SDL/filters.h"

#define STATE_SIZE    0x28000

bool exitFromXUI;
extern int IsCurrentlyInGame;
 
extern IDirect3DDevice9 *pDevice;
 
extern int run_loop;
extern bool exitFromXUI;
extern HXUIOBJ hRomListScene;
extern HXUIOBJ phObj;
extern bool paused;
extern bool wasPaused;
extern bool pauseNextFrame; 

extern char InGamePreview[MAX_PATH]; 
extern int emulating;
extern struct EmulatedSystem emulator;
extern t_config config;
extern int showSpeed;
extern enum Filter filter;
extern void sdlReadState(int num);
extern void sdlWriteState(int num);
extern int autoFrameSkip;
extern int frameSkip;

#define INGAME 0
#define PAUSED 1
#define BACKTOGAME 2
#define QUIT   3
 

// Handler for the XM_NOTIFY message
long CInGameOptions::OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled )
{
	if ( hObjPressed == m_SaveState)
	{
			const WCHAR * button_text = L"OK";
			 
			wchar_t romName[50];
 
		    sdlWriteState(0);
			 
			ShowMessageBoxEx(NULL,NULL,L"Visual Boy Advance 360 - Save State", L"Saved state successfully", 1, (LPCWSTR*)&button_text,NULL,  XUI_MB_CENTER_ON_PARENT, NULL); 
			 

			bHandled = TRUE;
			return S_OK;
			
        }
		else if (hObjPressed == m_LoadState)
		{
			int len = 0;
			XCONTENT_DATA contentData;

			const WCHAR * button_text = L"OK";

			sdlReadState(0);
 			 
			ShowMessageBoxEx(NULL,NULL,L"Visual Boy Advance 360 - Load State", L"Loaded state successfully", 1, (LPCWSTR*)&button_text,NULL,  XUI_MB_CENTER_ON_PARENT, NULL); 
			 

			bHandled = TRUE;
			return S_OK;			


		} 
		else if (hObjPressed == m_BackToGame)
		{
			paused = false;
			//wasPaused = true;				
			bHandled = TRUE;
			 
			return S_OK;

		}
		else if (hObjPressed == m_ExitGame)
		{	
			paused = false;
			emulating = 0;
			bHandled = TRUE;			 			     
			this->NavigateBack();
 			return S_OK;
		}
		else if (hObjPressed == m_AspectRatio)
		{

			if(m_AspectRatio.IsChecked()) {				 
				config.aspect = 1;
			} else {				 
				config.aspect = 0;
			}

			bHandled = TRUE;
			return S_OK;

		}
		else if (hObjPressed == m_Reset)
		{
			
			paused = false;
			emulator.emuReset();			
			bHandled = TRUE;
			return S_OK;
		}
		else if (hObjPressed == m_PointFiltering)
		{ 
			if(m_PointFiltering.IsChecked()) {				 
				config.pointfilter = 1;
			} else {				 
				config.pointfilter = 0;
			}
			bHandled = TRUE;
			return S_OK;

		}
		else if (hObjPressed == m_ShowFPS)
		{
			if(m_ShowFPS.IsChecked()) {				 
				config.showFPS = 1;
				showSpeed = 2;
			} else {				 
				config.showFPS = 0;
				showSpeed = 0;
			} 

			bHandled = TRUE;
			return S_OK;
		}
		else if (hObjPressed == m_SoundFilter)
		{
			 

			bHandled = TRUE;
			return S_OK;
		}
		else if (hObjPressed == m_Scale2x)
		{
			 
			if (m_Scale2x.IsChecked())
			{
				filter = kAdMame2x;		
				config.bilinear = 1;
			}
			else
			{
				filter = kStretch2x;
				config.bilinear = 0;
			}
			bHandled = TRUE;
			return S_OK;

		}
		else if (hObjPressed == m_MuteAudio)
		{
		 

			bHandled = TRUE;
			return S_OK;

		}
		else if (hObjPressed == m_TakePreview)
		{
			 
			  if (DoScreenshot(240, 160,  InGamePreview))
			  {
		 
 				const WCHAR * button_text = L"OK";				
 				ShowMessageBoxEx(NULL,NULL,L"Visual Boy Advance 360 - Take Preview", L"Preview Saved.", 1, (LPCWSTR*)&button_text,NULL,  XUI_MB_CENTER_ON_PARENT, NULL);
  
 				bHandled = TRUE;
 				return S_OK;
 			  } 

		}

 
        bHandled = TRUE;
        return S_OK;
    }


    //----------------------------------------------------------------------------------
    // Performs initialization tasks - retreives controls.
    //----------------------------------------------------------------------------------
long CInGameOptions::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
        // Retrieve controls for later use.
        GetChildById( L"XuiSaveStateButton", &m_SaveState );
        GetChildById( L"XuiLoadStateButton", &m_LoadState );
        GetChildById( L"XuiSoundFilt", &m_SoundFilter );		 
		GetChildById( L"XuiFilterScanlines", &m_Scanlines );			 
		GetChildById( L"XuiFilterSuperEagle", &m_SuperEagle );
		GetChildById( L"XuiButtonReset", &m_Reset);
		GetChildById( L"XuiDisplayFPS", &m_ShowFPS );
		GetChildById( L"XuiPreviewImage", &m_PreviewImage );
		GetChildById( L"XuiPreviewSmall", &m_PreviewSmallImage );
		GetChildById( L"XuiButtonBackToGame", &m_BackToGame);
		GetChildById( L"XuiButtonExitGame", &m_ExitGame);
		GetChildById( L"XuiAspectRatio", &m_AspectRatio);
		GetChildById( L"XuiPointFiltering", &m_PointFiltering);
		GetChildById( L"XuiMuteAudio", &m_MuteAudio);
		GetChildById( L"XuiButtonTakePreview", &m_TakePreview);
		GetChildById( L"XuiScale2x", &m_Scale2x);
	
	 

        return S_OK;
    }


HRESULT CInGameOptions::OnInGameMenu( int iVal1, int & bHandled )
{ 
	DeleteFile("cache:\\preview.png");

	if (DoScreenshot(240, 160,  "cache:\\preview.png"))
	{
		HRESULT hr;
 
		m_PreviewImage.DiscardResources(XUI_DISCARD_TEXTURES|XUI_DISCARD_VISUALS);
		m_PreviewSmallImage.DiscardResources(XUI_DISCARD_TEXTURES|XUI_DISCARD_VISUALS);

		hr = m_PreviewImage.SetBasePath(L"file://cache:/");
		hr = m_PreviewImage.SetImagePath(L"file://cache:/preview.png");
		hr = m_PreviewSmallImage.SetBasePath(L"file://cache:/");
		hr = m_PreviewSmallImage.SetImagePath(L"file://cache:/preview.png");

		bHandled = TRUE;
	}

	if (config.bilinear==1)
	{
		m_Scale2x.SetCheck(true);
	}
	else
	{
		m_Scale2x.SetCheck(false);
	}

	if (config.aspect==1)
	{
		m_AspectRatio.SetCheck(true);
	}
	else
	{
		m_AspectRatio.SetCheck(false);
	}

	if (config.showFPS==1)
	{
		m_ShowFPS.SetCheck(true);
	}
	else
	{
		m_ShowFPS.SetCheck(false);
	}

	if (config.pointfilter==1)
	{
		m_PointFiltering.SetCheck(true);
	}
	else
	{
		m_PointFiltering.SetCheck(false);
	}
 
	
    return( S_OK );
}

void InGameMenuFirstFunc(XUIMessage *pMsg, InGameMenuStruct* pData, int iVal1)
{
    XuiMessage(pMsg,XM_MESSAGE_ON_INGAME_MENU);
    _XuiMessageExtra(pMsg,(XUIMessageData*) pData, sizeof(*pData));

	
}

 
 