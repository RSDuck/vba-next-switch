#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>
#include <algorithm>
#include <new>
#include <iostream>
#include <vector>

#include "GeneralFunctions.h"
 

#ifndef ROMLIST_H
#define ROMLIST_H
 
#define XM_MESSAGE_ON_RESCAN_ROMS  XM_USER

extern "C" void config_default(void);
void AddToFavorites(char *path, char*file, DWORD numFavs);
void AddAllToFavorites(char *buff, DWORD size);
 
typedef struct
{
    char *szPath;    
}
InRescanRomsStruct;

typedef struct
{
	char path[MAX_PATH];
	char filename[MAX_PATH];
} FAVORITE;
 
void InRescanRomsFirstFunc(XUIMessage *pMsg, InRescanRomsStruct* pData, char *szPath);

// Define the message map macro
#define XUI_ON_XM_MESSAGE_ON_RESCAN_ROMS(MemberFunc)\
    if (pMessage->dwMessage == XM_MESSAGE_ON_RESCAN_ROMS)\
    {\
        InRescanRomsStruct *pData = (InRescanRomsStruct *) pMessage->pvData;\
        return MemberFunc(pData->szPath,  pMessage->bHandled);\
    }

class CRomList : CXuiListImpl
{
	public:

	XUI_IMPLEMENT_CLASS(CRomList, L"SnesRomList", XUI_CLASS_LIST);

	XUI_BEGIN_MSG_MAP()
		XUI_ON_XM_INIT(OnInit)
		XUI_ON_XM_GET_SOURCE_TEXT(OnGetSourceDataText)
		XUI_ON_XM_GET_ITEMCOUNT_ALL(OnGetItemCountAll)
		//XUI_ON_XM_GET_CURSEL(OnGetCurSel)
		XUI_ON_XM_NOTIFY( OnNotify )
		XUI_ON_XM_MESSAGE_ON_RESCAN_ROMS( OnRescanRoms )
		XUI_END_MSG_MAP()

		CRomList();
	long OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
	long OnNotify( XUINotify *hObj, BOOL& bHandled );
	long OnGetSourceDataText(XUIMessageGetSourceText *pGetSourceTextData, BOOL& bHandled);
	long OnGetItemCountAll(XUIMessageGetItemCount *pGetItemCountData, BOOL& bHandled);
	long OnRescanRoms ( char *szPath, BOOL& bHandled );

};

//--------------------------------------------------------------------------------------
// Scene implementation class.
//--------------------------------------------------------------------------------------
class CRomListScene : public CXuiSceneImpl
{
	protected:

	// Control and Element wrapper objects.
	CXuiControl m_AddToFavorites;
	CXuiControl m_PlayRom;
	CXuiControl m_Back; 
	CXuiElement m_PreviewImage;
	CXuiList m_RomList;
	//CXuiVideo   m_BackVideo;
	CXuiTextElement m_DeviceText;
	CXuiControl m_NextDevice;

	// Message map.
	XUI_BEGIN_MSG_MAP()
		XUI_ON_XM_INIT( OnInit )
		XUI_ON_XM_NOTIFY_PRESS( OnNotifyPress )
		XUI_END_MSG_MAP()

	public:
		long OnInit( XUIMessageInit* pInitData, BOOL& bHandled );
		long OnNotifyPress( HXUIOBJ hObjPressed, BOOL& bHandled );

	public:

		// Define the class. The class name must match the ClassOverride property
		// set for the scene in the UI Authoring tool.
		XUI_IMPLEMENT_CLASS( CRomListScene, L"RomListScene", XUI_CLASS_SCENE )
};


#endif
