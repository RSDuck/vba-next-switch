#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>


#ifndef GENERALUTIL_H
#define GENERALUTIL_H


/*
 * MessageBox() Flags
 */
#define MB_OK                       0x00000000L
#define MB_OKCANCEL                 0x00000001L
#define MB_ABORTRETRYIGNORE         0x00000002L
#define MB_YESNOCANCEL              0x00000003L
#define MB_YESNO                    0x00000004L
#define MB_RETRYCANCEL              0x00000005L
#define MB_ICONHAND                 0x00000010L
#define MB_ICONQUESTION             0x00000020L
#define MB_ICONEXCLAMATION          0x00000030L
#define MB_ICONASTERISK             0x00000040L
#if(WINVER >= 0x0400)
#define MB_USERICON                 0x00000080L
#define MB_ICONWARNING              MB_ICONEXCLAMATION
#define MB_ICONERROR                MB_ICONHAND
#endif /* WINVER >= 0x0400 */
#define MB_ICONINFORMATION          MB_ICONASTERISK
#define MB_ICONSTOP                 MB_ICONHAND
#define MB_DEFBUTTON1               0x00000000L
#define MB_DEFBUTTON2               0x00000100L
#define MB_DEFBUTTON3               0x00000200L
#if(WINVER >= 0x0400)
#define MB_DEFBUTTON4               0x00000300L
#endif /* WINVER >= 0x0400 */
#define MB_APPLMODAL                0x00000000L
#define MB_SYSTEMMODAL              0x00001000L
#define MB_TASKMODAL                0x00002000L
#if(WINVER >= 0x0400)
#define MB_HELP                     0x00004000L // Help Button
#endif /* WINVER >= 0x0400 */
#define MB_NOFOCUS                  0x00008000L
#define MB_SETFOREGROUND            0x00010000L
#define MB_DEFAULT_DESKTOP_ONLY     0x00020000L
#if(WINVER >= 0x0400)
#define MB_TOPMOST                  0x00040000L
#define MB_RIGHT                    0x00080000L
#define MB_RTLREADING               0x00100000L
#endif
 
long DXTRACE_ERR_MSGBOX( char *str, long hr);
long DXTRACE_ERR( char *str, long hr);
int MessageBox(HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType);

unsigned long CheckMenuItem(HMENU hmenu, UINT uIDCheckItem, UINT uCheck);

long LoadFile(const char * strFileName, VOID** ppFileData, unsigned long * pdwFileSize = NULL );
VOID UnloadFile( VOID* pFileData );

long LoadPixelShader( const CHAR* strFileName, LPDIRECT3DPIXELSHADER9* ppPS, LPD3DXCONSTANTTABLE* ppConstantTable = NULL );

LPCWSTR MultiCharToUniChar(char* mbString);

const char *GetFilename (char *path, const char *InFileName, char *fext);
void MapInternalDrives();
bool CreateDriveMapping(const char * szMappingName, const char * szDeviceName);
void CreateDirectoryAnyDepth(const wchar_t *path);

#endif
