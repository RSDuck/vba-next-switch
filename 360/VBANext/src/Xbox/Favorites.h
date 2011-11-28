//--------------------------------------------------------------------------------------
// XuiTutorial.cpp
//
// Shows how to display and use a simple XUI scene.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>
#include <algorithm>
#include <new>
#include <iostream>
#include <vector>
#include "RomList.h"
#include "GeneralFunctions.h"
 



#ifndef FAVORITES_H
#define FAVORITES_H
 
#define XM_MESSAGE_ON_RESCAN_FAVORITES  XM_USER+1

typedef struct
{
    char *szPath;
	char *szRom;

}
InRescanFavoriteStruct;

void InRescanFavoritesFirstFunc(XUIMessage *pMsg, InRescanFavoriteStruct* pData);
void BuildFavorites(void);


#define XUI_ON_XM_MESSAGE_ON_RESCAN_FAVORITES(MemberFavoritesFunc)\
    if (pMessage->dwMessage == XM_MESSAGE_ON_RESCAN_FAVORITES)\
    {\
        InRescanFavoriteStruct *pData = (InRescanFavoriteStruct *) pMessage->pvData;\
        return MemberFavoritesFunc(pData->szPath, pData->szRom,  pMessage->bHandled);\
	}

extern "C" void config_default(void);
 
 
class CFavoritesList : CXuiListImpl
{
public:

	XUI_IMPLEMENT_CLASS(CFavoritesList, L"FavoritesRomList", XUI_CLASS_LIST);

	
 
	XUI_BEGIN_MSG_MAP()
		XUI_ON_XM_INIT(OnInit)
		XUI_ON_XM_GET_SOURCE_TEXT(OnGetSourceDataText)
		XUI_ON_XM_GET_ITEMCOUNT_ALL(OnGetItemCountAll)
		//XUI_ON_XM_GET_CURSEL(OnGetCurSel)
		XUI_ON_XM_NOTIFY( OnNotify )		
		XUI_ON_XM_MESSAGE_ON_RESCAN_FAVORITES( OnRescanRoms )
	XUI_END_MSG_MAP()

	CFavoritesList();
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
	HRESULT OnNotify( XUINotify *hObj, BOOL& bHandled );
	HRESULT OnGetSourceDataText(XUIMessageGetSourceText *pGetSourceTextData, BOOL& bHandled);
	HRESULT OnGetItemCountAll(XUIMessageGetItemCount *pGetItemCountData, BOOL& bHandled);
	HRESULT OnRescanRoms( char *szPath, char *szRom,  BOOL& bHandled );

	
	 
 
	
 
};

//--------------------------------------------------------------------------------------
// Scene implementation class.
//--------------------------------------------------------------------------------------
class CFavoritesListScene : public CXuiSceneImpl
{

protected:

    // Control and Element wrapper objects.
    CXuiControl m_RemoveFavorites;
    CXuiControl m_PlayRom;
	CXuiControl m_Back; 
	CXuiElement m_PreviewImage;
	CXuiList m_RomList;
	CXuiVideo   m_BackVideo;
	CXuiTextElement m_DeviceText;
	CXuiControl m_NextDevice;
 
   

    // Message map.
    XUI_BEGIN_MSG_MAP()
		XUI_ON_XM_INIT( OnInit )
		XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
    XUI_END_MSG_MAP()

	


 
public:
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
	HRESULT OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled );
 
public:

    // Define the class. The class name must match the ClassOverride property
    // set for the scene in the UI Authoring tool.
    XUI_IMPLEMENT_CLASS( CFavoritesListScene, L"FavoritesListScene", XUI_CLASS_SCENE )
};


#endif