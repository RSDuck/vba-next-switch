#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>
#include <algorithm>
#include <new>
#include <iostream>
#include <vector>

#include "GeneralFunctions.h"

#ifndef INGAMEOPTIONS_H
#define INGAMEOPTIONS_H
 

#define XM_MESSAGE_ON_INGAME_MENU  XM_USER

typedef struct
{
    int iVal1;    
}
InGameMenuStruct;


void InGameMenuFirstFunc(XUIMessage *pMsg, InGameMenuStruct* pData, int iVal1);

// Define the message map macro
#define XUI_ON_XM_MESSAGE_ON_INGAME_MENU(MemberFunc)\
    if (pMessage->dwMessage == XM_MESSAGE_ON_INGAME_MENU)\
    {\
        InGameMenuStruct *pData = (InGameMenuStruct *) pMessage->pvData;\
        return MemberFunc(pData->iVal1,  pMessage->bHandled);\
    }

//--------------------------------------------------------------------------------------
// Scene implementation class.
//--------------------------------------------------------------------------------------
class CInGameOptions : public CXuiSceneImpl
{
	protected:

	// Control and Element wrapper objects.
	CXuiControl m_SaveState;
	CXuiControl m_LoadState;

	CXuiControl m_BackToGame;

	CXuiControl m_ExitGame;
	CXuiControl m_TakePreview;
	CXuiControl m_Reset;


	CXuiControl m_Simple2x;
	CXuiControl m_Scanlines;
	CXuiCheckbox m_Scale2x;
	CXuiControl m_SuperEagle;
	CXuiCheckbox m_SoundFilter;
	CXuiCheckbox m_ShowFPS;

	CXuiImageElement m_PreviewImage;
	CXuiImageElement m_PreviewSmallImage;

	CXuiCheckbox m_AspectRatio;
	CXuiCheckbox m_PointFiltering;
	CXuiCheckbox m_MuteAudio;

	CXuiRadioGroup m_FilterGroup;

	CRITICAL_SECTION m_CriticalSection;


	// Message map.
	XUI_BEGIN_MSG_MAP()
		XUI_ON_XM_INIT( OnInit )
		XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )	
		XUI_ON_XM_MESSAGE_ON_INGAME_MENU( OnInGameMenu )
		XUI_END_MSG_MAP()
	public:
		long OnInit( XUIMessageInit* pInitData, int& bHandled );
		long OnNotifyPress( HXUIOBJ hObjPressed, int& bHandled );
		long OnInGameMenu ( int iVal1, int& bHandled );
		// Define the class. The class name must match the ClassOverride property
		// set for the scene in the UI Authoring tool.
		XUI_IMPLEMENT_CLASS( CInGameOptions, L"InGameOptions", XUI_CLASS_SCENE )
};

#endif
