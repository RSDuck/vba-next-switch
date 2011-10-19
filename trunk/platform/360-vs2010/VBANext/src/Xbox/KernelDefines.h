//This is the code for KernelDefines.h

#ifndef _KERNEL_DEFINES_H
#define _KERNEL_DEFINES_H
 
#ifdef _XBOX
#else
#include <windows.h>
#endif

#define CONSTANT_OBJECT_STRING(s)   { strlen( s ) / sizeof( OCHAR ), (strlen( s ) / sizeof( OCHAR ))+1, s }

//#define IN
//#define OUT
//#define OPTIONAL
#define STATUS_SUCCESS	0
#define FILE_SYNCHRONOUS_IO_NONALERT	0x20
#define OBJ_CASE_INSENSITIVE			0x40

typedef long			NTSTATUS;
typedef ULONG			ACCESS_MASK;

typedef struct _STRING {
    USHORT Length;
    USHORT MaximumLength;
    PCHAR Buffer;
} STRING, *PSTRING;
typedef struct _CSTRING {
	USHORT Length;
	USHORT MaximumLength;
	CONST char *Buffer;
} CSTRING, *PCSTRING;
typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;
typedef STRING			OBJECT_STRING;
typedef CSTRING			COBJECT_STRING;
typedef PSTRING			POBJECT_STRING;
typedef PCSTRING		PCOBJECT_STRING;
typedef STRING			OEM_STRING;
typedef PSTRING			POEM_STRING;
typedef CHAR			OCHAR;
typedef CHAR*			POCHAR;
typedef PSTR			POSTR;
typedef PCSTR			PCOSTR;
typedef CHAR*			PSZ;
typedef CONST CHAR*		PCSZ;
typedef STRING			ANSI_STRING;
typedef PSTRING			PANSI_STRING;
typedef CSTRING			CANSI_STRING;
typedef PCSTRING		PCANSI_STRING;
#define ANSI_NULL		((CHAR)0)     // winnt
typedef CONST UNICODE_STRING*	PCUNICODE_STRING;
#define UNICODE_NULL			((WCHAR)0) // winnt

#define OTEXT(quote) __OTEXT(quote)


typedef struct _IO_STATUS_BLOCK {
    union {
        NTSTATUS Status;
        PVOID Pointer;
    } st;
    ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef VOID (NTAPI *PIO_APC_ROUTINE) (
    IN PVOID ApcContext,
    IN PIO_STATUS_BLOCK IoStatusBlock,
    IN ULONG Reserved
    );

typedef struct _OBJECT_ATTRIBUTES {
    HANDLE RootDirectory;
    POBJECT_STRING ObjectName;
    ULONG Attributes;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;


// returned by a call to 'NtQueryInformationFile' with 0x22 = FileNetworkOpenInformation
typedef struct _FILE_NETWORK_OPEN_INFORMATION {
  LARGE_INTEGER  CreationTime;
  LARGE_INTEGER  LastAccessTime;
  LARGE_INTEGER  LastWriteTime;
  LARGE_INTEGER  ChangeTime;
  LARGE_INTEGER  AllocationSize;
  LARGE_INTEGER  EndOfFile;
  ULONG  FileAttributes;
} FILE_NETWORK_OPEN_INFORMATION, *PFILE_NETWORK_OPEN_INFORMATION;

#endif	//_KERNEL_DEFINES_H