/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002  Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@libsdl.org
*/

#ifdef SAVE_RCSID
static char rcsid =
 "@(#) $Id: SDL_syscdrom.c,v 1.4 2002/03/06 11:23:02 slouken Exp $";
#endif

/* Stub functions for system-level CD-ROM audio control */

#include <xtl.h>
#include "undocumented.h"
#include "SDL_cdrom.h"
#include "SDL_syscdrom.h"

// CD audio type conversions

#define CDAUDIO_BYTES_PER_FRAME         2352
#define CDAUDIO_BYTES_PER_SECOND        176400
#define CDAUDIO_BYTES_PER_MINUTE        10584000

#define CDAUDIO_FRAMES_PER_SECOND       75
#define CDAUDIO_FRAMES_PER_MINUTE       4500

#define TOC_DATA_TRACK              (0x04)
#define TOC_LAST_TRACK              (0xaa)

#define FRAMES_PER_CHUNK (CDAUDIO_FRAMES_PER_SECOND)
#define CD_AUDIO_SEGMENTS_PER_BUFFER ((4*CDAUDIO_FRAMES_PER_SECOND)/FRAMES_PER_CHUNK)
#define BYTES_PER_CHUNK (FRAMES_PER_CHUNK * CDAUDIO_BYTES_PER_FRAME)


// MCI time format conversion macros

#define MCI_MSF_MINUTE(msf)             ((BYTE)(msf))
#define MCI_MSF_SECOND(msf)             ((BYTE)(((WORD)(msf)) >> 8))
#define MCI_MSF_FRAME(msf)              ((BYTE)((msf)>>16))

#define MCI_MAKE_MSF(m, s, f)           ((DWORD)(((BYTE)(m) | \
                                        ((WORD)(s)<<8)) | \
                                        (((DWORD)(BYTE)(f))<<16)))

#define CTLCODE(DeviceType, Function, Method, Access) ( ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method)  ) 
#define FSCTL_DISMOUNT_VOLUME  CTLCODE( FILE_DEVICE_FILE_SYSTEM, 0x08, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define CDDA_MAX_FRAMES_PER_READ 16
#define CDDA_BUFFER_SIZE (CDDA_MAX_FRAMES_PER_READ * CDAUDIO_BYTES_PER_FRAME)

static HANDLE hDevice;
static HANDLE hCDDAThread;

HRESULT Remount(void);
HRESULT Mount(void);
HRESULT Unmount(void);
void cdrom_cdda_play(int start, int length);
void cddapause(BOOL state);
int cddaprocess(void);
int ReadSectorCDDA(HANDLE hDevice, DWORD dwSector, LPSTR lpczBuffer);

DWORD TrackAddr [100];

DWORD dwStartPosition;
DWORD dwStopPosition;
LPVOID pvBuffer;
DWORD ThreadID; 
 
int LastTrack;
 
//extern LPDIRECTSOUND g_sound;
 

// the local buffer is what the stream buffer feeds from
// note that this needs to be large enough to buffer at frameskip 11
// for 30fps games like Tapper; we will scale the value down based
// on the actual framerate of the game
#define MAX_BUFFER_SIZE			(128 * 1024 * 4)
 
int					attenuation ;
int					current_adjustment ;
 
// global sample tracking
double				samples_per_frame;
double				samples_left_over;
UINT32				samples_this_frame;
UINT32              samples_to_read ;
signed short		*m_mixbuf ;
int					m_bDanger ;
int					m_nVolume ;

// sound buffers
//LPDIRECTSOUNDBUFFER8	stream_buffer ;
UINT32				stream_buffer_size;
UINT32				stream_buffer_in;
UINT32				m_totalBytesWritten ;
byte				*m_pSoundBufferData ;
DWORD				m_dwWritePos ;
DWORD				m_sectorFrom;
DWORD				m_sectorTo ;
DWORD				m_currentSector ;
int                 m_repeat ;
int					m_bPaused ;
int					m_bDone ;
 
// descriptors and formats
//DSBUFFERDESC			stream_desc;
//WAVEFORMATEX			stream_format;

// sample rate adjustments
int					lower_thresh;
int					upper_thresh;

static int	nCDStatus;

/* The system-dependent CD control functions */
static const char *SDL_SYS_CDName(int drive);
static int SDL_SYS_CDOpen(int drive);
static int SDL_SYS_CDGetTOC(SDL_CD *cdrom);
static CDstatus SDL_SYS_CDStatus(SDL_CD *cdrom, int *position);
static int SDL_SYS_CDPlay(SDL_CD *cdrom, int start, int length);
static int SDL_SYS_CDPause(SDL_CD *cdrom);
static int SDL_SYS_CDResume(SDL_CD *cdrom);
static int SDL_SYS_CDStop(SDL_CD *cdrom);
static int SDL_SYS_CDEject(SDL_CD *cdrom);
static void SDL_SYS_CDClose(SDL_CD *cdrom);
DWORD GetTrackFrame(int nTrack);

__inline DWORD MsfToFrames(DWORD dwMsf)
{
    return MCI_MSF_MINUTE(dwMsf) * CDAUDIO_FRAMES_PER_MINUTE +
           MCI_MSF_SECOND(dwMsf) * CDAUDIO_FRAMES_PER_SECOND +
           MCI_MSF_FRAME(dwMsf);
}

__inline DWORD FramesToMsf(DWORD dwFrames)
{
    return MCI_MAKE_MSF(
        dwFrames / CDAUDIO_FRAMES_PER_MINUTE,
        (dwFrames % CDAUDIO_FRAMES_PER_MINUTE) / CDAUDIO_FRAMES_PER_SECOND,
        (dwFrames % CDAUDIO_FRAMES_PER_MINUTE) % CDAUDIO_FRAMES_PER_SECOND);
}

__inline DWORD TocValToMsf(LPBYTE ab)
{
    return MCI_MAKE_MSF(ab[1], ab[2], ab[3]);
}

__inline DWORD TocValToFrames(LPBYTE ab)
{
    return MsfToFrames(TocValToMsf(ab));
}

int SDL_SYS_CDInit(void)
{
	HRESULT result;
 

	/* Fill in our driver capabilities */
	SDL_CDcaps.Name = SDL_SYS_CDName;
	SDL_CDcaps.Open = SDL_SYS_CDOpen;
	SDL_CDcaps.GetTOC = SDL_SYS_CDGetTOC;
	SDL_CDcaps.Status = SDL_SYS_CDStatus;
	SDL_CDcaps.Play = SDL_SYS_CDPlay;
	SDL_CDcaps.Pause = SDL_SYS_CDPause;
	SDL_CDcaps.Resume = SDL_SYS_CDResume;
	SDL_CDcaps.Stop = SDL_SYS_CDStop;
	SDL_CDcaps.Eject = SDL_SYS_CDEject;
	SDL_CDcaps.Close = SDL_SYS_CDClose;

	SDL_numcds++;

/*
	m_totalBytesWritten = 0 ;
	m_dwWritePos = 0 ;
	memset( m_pSoundBufferData, 0, stream_buffer_size ) ;
	m_bDanger = 0 ;
	m_nVolume = DSBVOLUME_MAX ;
	m_sectorFrom = 0 ;
	m_sectorTo =0 ;
	m_repeat = 0 ;
	m_bPaused = 1 ;
	m_bDone = 1 ;
	hDevice = NULL;

	current_adjustment = 0 ;
	m_totalBytesWritten = 0 ;

	if (stream_buffer)
		IDirectSoundBuffer8_Release(stream_buffer);

	// make a format description for what we want
	stream_format.wBitsPerSample	= 16;
	stream_format.wFormatTag		= WAVE_FORMAT_PCM;
	stream_format.nChannels			= 2;
	stream_format.nSamplesPerSec	= 44100;
	stream_format.nBlockAlign		= stream_format.wBitsPerSample * stream_format.nChannels / 8;
	stream_format.nAvgBytesPerSec	= stream_format.nSamplesPerSec * stream_format.nBlockAlign;

	// compute the buffer sizes
	stream_buffer_size = (UINT64)MAX_BUFFER_SIZE ;
	stream_buffer_size = (stream_buffer_size * stream_format.nBlockAlign) / 4;
	stream_buffer_size = (stream_buffer_size * 30) / 60 ;
	stream_buffer_size = (stream_buffer_size / 2940) * 2940;
	stream_buffer_size = 9500 ;

	stream_buffer_in = 0 ;
 
	// create a buffer desc for the stream buffer
	stream_desc.dwSize				= sizeof(stream_desc);
	stream_desc.dwFlags				= 0 ;
	stream_desc.dwBufferBytes 		= 0; //we'll specify our own data
	stream_desc.lpwfxFormat			= &stream_format;
 
	// create the stream buffer
	if ((result = IDirectSound8_CreateSoundBuffer(g_sound, &stream_desc, &stream_buffer, NULL)) != DS_OK)
	{
		stream_buffer = NULL ;
		return (-1);
	}
	
	m_pSoundBufferData = (byte*)malloc( stream_buffer_size ) ;

	IDirectSoundBuffer_SetBufferData(stream_buffer, m_pSoundBufferData, stream_buffer_size ) ;
	IDirectSoundBuffer_SetPlayRegion(stream_buffer,  0, stream_buffer_size ) ;
	IDirectSoundBuffer_SetLoopRegion(stream_buffer,  0, stream_buffer_size ) ;

	memset( m_pSoundBufferData, 0, stream_buffer_size);

	IDirectSoundBuffer_SetVolume(stream_buffer, DSBVOLUME_MAX );
	IDirectSoundBuffer_SetCurrentPosition( stream_buffer, 0 ) ;

	cddapause(FALSE) ;
	m_bDone = 0 ;
     */
	nCDStatus = CD_STOPPED;

	return (0);
}

void SDL_SYS_CDQuit(void)
{
	if (hDevice)
	{
		CloseHandle(hDevice);
		hDevice=NULL;
	}

	if (m_pSoundBufferData)
	{
		free(m_pSoundBufferData);
		m_pSoundBufferData = NULL;
	}
}

static int SDL_SYS_CDEject(SDL_CD *cdrom)
{
	/*
	if (stream_buffer)
	{
		IDirectSoundBuffer_Stop(stream_buffer) ;
		m_bDone = 1 ;
		m_bPaused = 1 ;
	}*/

	//HalWriteSMBusValue(0x20, 0x0C, FALSE, 0);  // eject tray
	return S_OK;
}

static void SDL_SYS_CDClose(SDL_CD *cdrom)
{

	cddapause(TRUE) ;
 
	/*
	if (stream_buffer)
		IDirectSoundBuffer8_Release(stream_buffer);

	if (m_pSoundBufferData)
	{
		free(m_pSoundBufferData);
		m_pSoundBufferData = NULL;
	}
	*/
	if (hDevice)
	{
		CloseHandle(hDevice);
		hDevice=NULL;
	}
 
}


HRESULT Remount(void)
{
	ANSI_STRING filename;
	OBJECT_ATTRIBUTES attributes;
	IO_STATUS_BLOCK status;
	HANDLE hDevice;
	NTSTATUS error;
	DWORD dummy;

	char szSourceDevice[48];
	strcpy(szSourceDevice,"\\Device\\Cdrom0");

	Unmount();
	
	RtlInitAnsiString(&filename, szSourceDevice);
	InitializeObjectAttributes(&attributes, &filename, OBJ_CASE_INSENSITIVE, NULL);

	if (NT_SUCCESS(error = NtCreateFile(&hDevice, GENERIC_READ |
		SYNCHRONIZE | FILE_READ_ATTRIBUTES, &attributes, &status, NULL, 0,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN,
		FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT)))
	{

		if (!DeviceIoControl(hDevice, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dummy, NULL))
		{
			CloseHandle(hDevice);
			return E_FAIL;
		}

		CloseHandle(hDevice);
	}
	
	Mount();

	return S_OK;
}

HRESULT Mount(void)
{
	STRING DeviceName =
	{
		strlen("\\Device\\Cdrom0"),
		strlen("\\Device\\Cdrom0") + 1,
		"\\Device\\Cdrom0"
	};

	STRING LinkName =
	{
		strlen("\\??\\R:"),
		strlen("\\??\\R:") + 1,
		"\\??\\R:"
	};

//	IoCreateSymbolicLink(&LinkName, &DeviceName);

	return S_OK;
}

HRESULT Unmount(void)
{
	STRING LinkName =
	{
		strlen("\\??\\R:"),
		strlen("\\??\\R:") + 1,
		"\\??\\R:"
	};

//	IoDeleteSymbolicLink(&LinkName);
	
	return S_OK;
}


static int SDL_SYS_CDGetTOC(SDL_CD *cdrom)
{
	HRESULT hr;
	int nRetry;
	CDROM_TOC toc;
	DWORD dummy;
	DWORD i;
	BOOL fFoundEndTrack = FALSE;

	ANSI_STRING filename;
	OBJECT_ATTRIBUTES attributes;
	IO_STATUS_BLOCK status;
	 
	memset(&toc, 0x00, sizeof(toc));

	if (hDevice)
	{

		CloseHandle(hDevice);
 
		RtlInitAnsiString(&filename,"\\Device\\Cdrom0");
		InitializeObjectAttributes(&attributes, &filename, OBJ_CASE_INSENSITIVE, NULL);
		NtCreateFile(&hDevice,
		            GENERIC_READ |SYNCHRONIZE | FILE_READ_ATTRIBUTES, 
					&attributes, 
					&status, 
					NULL, 
					0,
					FILE_SHARE_READ,
					FILE_OPEN,	
					FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT);

		

	}

	for (nRetry = 0; nRetry < 1; nRetry += 1)
    {
		hr = DeviceIoControl(hDevice, IOCTL_CDROM_READ_TOC, NULL, 0, &toc, sizeof (toc), &dummy, NULL);
		Sleep(10);
	}

	if (toc.FirstTrack > toc.LastTrack)
		return -1;
	 
	cdrom->numtracks = toc.LastTrack;

	for (i = 0; i <= (DWORD)(toc.LastTrack - toc.FirstTrack + 1); i++)
	{
		TrackAddr[i] = TocValToFrames(toc.TrackData[i].Address);
		cdrom->track[i].offset = TrackAddr[i];
		cdrom->track[i].type   = SDL_AUDIO_TRACK;
		cdrom->track[i].id     = (int)i;
	
		// Break out if we find a last track marker.
		if (toc.TrackData[i].TrackNumber == TOC_LAST_TRACK)
        {
			fFoundEndTrack = TRUE;
            break;
        }

		// Dont Break out if we find a data track.
        if ((toc.TrackData[i].Control & TOC_DATA_TRACK) != 0)
        {
            // Knock off 2.5 minutes to account for the final leadin.
            toc.TrackData[i].Address[1] -= 2;
            toc.TrackData[i].Address[2] += 30;

            if (toc.TrackData[i].Address[2] < 60)
                toc.TrackData[i].Address[1] -= 1;
            else
                toc.TrackData[i].Address[2] -= 60;

           
			TrackAddr[i] = TocValToFrames(toc.TrackData[i].Address);
			cdrom->track[i].type   = SDL_DATA_TRACK;
   
        } 


	}

	LastTrack = i;

	for (i = 0; i <= (toc.LastTrack - toc.FirstTrack + 1); i++)
	{
		if (i == toc.LastTrack)
			cdrom->track[i].length = (int)(TrackAddr[i]);
		else
			cdrom->track[i].length = (int)(TrackAddr[i + 1]) - (int)(TrackAddr[i]);

	}

	return 0;
}


/* Get CD-ROM status */
static CDstatus SDL_SYS_CDStatus(SDL_CD *cdrom, int *position)
{
	if (position)
		*position = 0;

	return nCDStatus;
}

static int SDL_SYS_CDResume(SDL_CD *cdrom)
{
	cddapause(FALSE);
	m_bPaused = 0;
	hCDDAThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)cddaprocess, (LPVOID)NULL,0, &ThreadID);
	return 1;
}

static int SDL_SYS_CDStop(SDL_CD *cdrom)
{
 	cddapause(TRUE);
	m_bDone = 1 ;
	m_bPaused = 0 ;
	return 1;
}

static int SDL_SYS_CDPlay(SDL_CD *cdrom, int start, int length)
{ 
	SDL_SYS_CDStop(NULL);	
	Sleep(600);
	cdrom_cdda_play( start, start+length);
	return 0;
}

static const char *SDL_SYS_CDName(int drive)
{
	return("R:\\");
}

static int SDL_SYS_CDPause(SDL_CD *cdrom)
{
	cddapause(TRUE);
	return 0;
}

static int SDL_SYS_CDOpen(int drive)
{
	ANSI_STRING filename;
	OBJECT_ATTRIBUTES attributes;
	IO_STATUS_BLOCK status;
	NTSTATUS error;

	//Remount();
	//Unmount();

	RtlInitAnsiString(&filename,"\\Device\\Cdrom0");
	InitializeObjectAttributes(&attributes, &filename, OBJ_CASE_INSENSITIVE, NULL);
	if (!NT_SUCCESS(error = NtCreateFile(&hDevice,
		            GENERIC_READ |SYNCHRONIZE | FILE_READ_ATTRIBUTES, 
					&attributes, 
					&status, 
					NULL, 
					0,
					FILE_SHARE_READ,
					FILE_OPEN,	
					FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT)))
	{
		return 0;
	}

	return (int)hDevice;
}
 
void cdrom_cdda_play(int start, int length)
{
	
	m_dwWritePos = 0 ;

	m_repeat = 0 ;
	m_sectorFrom = start-150 ;
	m_sectorTo = length-150 ;
	m_currentSector = m_sectorFrom ;
	m_bDone = 0;
	
	/*
	 
	if (stream_buffer)
	{
		IDirectSoundBuffer_SetPlayRegion(stream_buffer,  0, stream_buffer_size ) ;
		IDirectSoundBuffer_SetLoopRegion(stream_buffer,  0, stream_buffer_size ) ;
	
		memset( m_pSoundBufferData, 0, stream_buffer_size);

		IDirectSoundBuffer_SetVolume(stream_buffer, DSBVOLUME_MAX );
		IDirectSoundBuffer_SetCurrentPosition( stream_buffer, 0 ) ;
	}
	*/
	cddapause(FALSE);

	hCDDAThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)cddaprocess, (LPVOID)NULL,0, &ThreadID);
}
 

void cddapause(BOOL state)
{
	 DWORD BufferStatus;
/*
	if (stream_buffer)
	{
		if (state)
		{
			IDirectSoundBuffer_Stop(stream_buffer) ;

			BufferStatus = DSBSTATUS_PLAYING;
            while (BufferStatus == DSBSTATUS_PLAYING)
                IDirectSoundBuffer_GetStatus(stream_buffer,&BufferStatus);

			m_bPaused = TRUE ;
			nCDStatus = CD_PAUSED;
		}
		else
		{
			IDirectSoundBuffer_Play(stream_buffer,  0, 0, DSBPLAY_LOOPING ) ;
			m_bPaused = FALSE ;
			nCDStatus = CD_PLAYING;
		}
	}
	*/
}

int cddaprocess(void)
{
	unsigned char sndbuf[10*1024] ;
 
	int            datalen ;
	DWORD          playPos  ;
	DWORD          bytesToEnd ;
	DWORD          distWritePlay ;
 
	memset(sndbuf,0x00, sizeof(sndbuf));
	
	/*
	while ( !m_bPaused && !m_bDone  )
	{
		 
		IDirectSoundBuffer_GetCurrentPosition(stream_buffer,  &playPos, NULL ) ;
	
		if ( m_dwWritePos > playPos )
			distWritePlay = m_dwWritePos - playPos ;
		else
			distWritePlay = ( stream_buffer_size - playPos ) + m_dwWritePos ;

		if ( distWritePlay < 4704 )
		{
			if ( m_currentSector > m_sectorTo )
			{
				if ( m_repeat )
					m_currentSector = m_sectorFrom ;
				else
				{
					m_bDone = 1 ;
					cddapause(TRUE);
					return 0;
				}
			}

			if ( ReadSectorCDDA( hDevice, m_currentSector++, (LPSTR)sndbuf ) != 2352 )
			{
				if ( m_repeat )
				{
					m_sectorTo = m_currentSector-2 ;
					m_currentSector = m_sectorFrom ;
					cddapause(TRUE);
					return 0;
				}
				else
				{
					m_bDone = 1 ;
					cddapause(TRUE);
					return 0;
				}
			}

			datalen = 2352 ;

			if ( m_currentSector > m_sectorTo )
			{
				if ( m_repeat )
					m_currentSector = m_sectorFrom ;
				else
				{
					m_bDone = 1 ;
					cddapause(TRUE);
					return 0;
				}
			}


			if ( m_currentSector > m_sectorTo )
			{
				if ( m_repeat )
				{
					m_currentSector = m_sectorFrom ;
				}
				else
				{
					m_bDone = 1 ;
					cddapause(TRUE);
					return 0;
				}
			}

			if ( ReadSectorCDDA( hDevice, m_currentSector++, (LPSTR)(sndbuf+2352) ) != 2352 )
			{
				if ( m_repeat )
				{
					m_sectorTo = m_currentSector-2 ;
					m_currentSector = m_sectorFrom ;
					cddapause(TRUE);
					return 0;
				}
				else
				{
					m_bDone = 1 ;
					cddapause(TRUE);
					return 0;
				}
			}

			datalen += 2352 ; 
 
			bytesToEnd = stream_buffer_size - m_dwWritePos ;

			if ( (DWORD)datalen > bytesToEnd )
			{
				memcpy( m_pSoundBufferData + m_dwWritePos, sndbuf, bytesToEnd ) ;
				memcpy( m_pSoundBufferData, sndbuf + bytesToEnd, datalen - bytesToEnd ) ;
			}
			else
			{
				memcpy( m_pSoundBufferData + m_dwWritePos, sndbuf, datalen ) ;
			}

			m_dwWritePos = ( m_dwWritePos + datalen ) % stream_buffer_size ;
			m_totalBytesWritten += datalen ;

		}	
	 
	}	*/
	return 0 ;
}


int ReadSectorCDDA(HANDLE hDevice, DWORD dwSector, LPSTR lpczBuffer)
{
	/*DWORD dwBytesReturned;
	RAW_READ_INFO rawRead;
	int i = 0;

	rawRead.DiskOffset.QuadPart = 2048 * dwSector;
	rawRead.SectorCount = 1;
	rawRead.TrackMode = CDDA;

	for (i=0; i < 5; i++)
	{
		if( DeviceIoControl( hDevice,
			IOCTL_CDROM_RAW_READ,
			&rawRead,
			sizeof(RAW_READ_INFO),
			lpczBuffer,
			CDAUDIO_BYTES_PER_FRAME,
			&dwBytesReturned,
			NULL ) != 0 )
		{
			return CDAUDIO_BYTES_PER_FRAME;
		}
	}*/
	return -1;
}