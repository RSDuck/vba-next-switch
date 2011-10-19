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
 "@(#) $Id: SDL_xboxaudio.h,v 1.1 2003/07/18 15:19:33 lantus Exp $";
#endif

#ifndef _SDL_lowaudio_h
#define _SDL_lowaudio_h

#include <xtl.h>
#include <xaudio2.h>

#include "SDL_sysaudio.h"

/* Hidden "this" pointer for the video functions */
#define _THIS	SDL_AudioDevice *this

/* The DirectSound objects */
struct SDL_PrivateAudioData {
	IXAudio2 *sound;
	IXAudio2MasteringVoice* masterVoice;
	IXAudio2SourceVoice* mixbuf;
	int NUM_BUFFERS;
	int mixlen, silence;
	int currentBuffer;
	DWORD playing;
	Uint8 *locked_buf;
	BYTE* pAudioBuffers;
	HANDLE audio_event;
};

/* Old variable names */
#define sound			(this->hidden->sound)
#define masterVoice		(this->hidden->masterVoice)
#define mixbuf			(this->hidden->mixbuf)
#define NUM_BUFFERS		(this->hidden->NUM_BUFFERS)
#define mixlen			(this->hidden->mixlen)
#define silence			(this->hidden->silence)
#define playing			(this->hidden->playing)
#define locked_buf		(this->hidden->locked_buf)
#define audio_event		(this->hidden->audio_event)
#define pAudioBuffers	(this->hidden->pAudioBuffers)
#define currentBuffer	(this->hidden->currentBuffer)

#endif /* _SDL_lowaudio_h */

