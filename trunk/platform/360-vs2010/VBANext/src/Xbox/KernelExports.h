//This is the code for KernelExports.h

#include "KernelDefines.h"

extern "C"
{
	NTSTATUS ObCreateSymbolicLink( IN PANSI_STRING SymbolicLinkName, 
		IN PANSI_STRING DeviceName );

	// init string
	VOID RtlInitAnsiString(
		PANSI_STRING DestinationString,
		PCSZ SourceString );
}