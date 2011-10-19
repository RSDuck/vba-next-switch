/* CXAudio2.cpp - written by OV2 */

#include "CXaudio2.h"
#include "Favorites.h"
#include <process.h>

#define SOUND_SAMPLES_SIZE  2048

/* CXAudio2
	Implements audio output through XAudio2.
	Basic idea: one master voice and one source voice are created;
	the source voice consumes buffers queued by PushBuffer, plays them through
	the master voice and calls OnBufferEnd after each buffer.
	A mixing thread runs in a loop and waits for signals from OnBufferEnd to fill
	a new buffer and push it to the source voice.
*/
typedef struct
{
  int sample_rate;  /* Sample rate (8000-48000) */
  int enabled;      /* 1= sound emulation is enabled */
  int buffer_size;  /* Size of sound buffer (in bytes) */
  int16 *buffer[2]; /* Signed 16-bit stereo sound data */
  struct
  {
    int pos;
    int16 *buffer[2];
  } fm;
  struct
  {
    int pos;
    int16 *buffer;
  } psg;
} t_snd;

#define SAMPLERATE 48000
extern "C" t_snd snd;
extern "C" t_config config;
extern "C" void audio_update (int len);
extern "C" uint8 vdp_rate;
extern "C" void audio_shutdown (void);

/*  Construction/Destruction
*/
CXAudio2::CXAudio2(void)
{

	exitThread = false;
	initDone = false;
	emptyBuffers = 0;
}

CXAudio2::~CXAudio2(void)
{
	DeInitXAudio2();
}

/*  CXAudio2::InitXAudio2
initializes the XAudio2 object and starts the mixing thread in a waiting state
-----
returns true if successful, false otherwise
*/
bool CXAudio2::InitXAudio2(void)
{
	if(initDone)
		return true;

	XAudio2Create( &pXAudio2, 0, XboxThread2 );
	//eventHandle = CreateEvent(NULL,0,0,NULL);
	//threadHandle = (HANDLE)_beginthreadex(NULL,0,&SoundThread,(void *)this,0,NULL);
 
	initDone = true;

	InitializeCriticalSection(&audSection);
	return true;
}

/*  CXAudio2::InitVoices
initializes the voice objects with the current audio settings
-----
returns true if successful, false otherwise
*/
bool CXAudio2::InitVoices(void)
{

	return true;
}

/*  CXAudio2::DeInitXAudio2
deinitializes all objects and stops the mixing thread
*/
void CXAudio2::DeInitXAudio2(void)
{
	initDone = false;
	DeInitVoices();	
	if(threadHandle) {
		exitThread = true;
		SetEvent(eventHandle);							// signal the thread in case it is waiting
		WaitForSingleObject(threadHandle,INFINITE);		// wait for the thread to exit gracefully
		exitThread = false;
		CloseHandle(threadHandle);
		threadHandle = NULL;
	}
	if(eventHandle) {
		CloseHandle(eventHandle);
		eventHandle = NULL;
	}
	if(pXAudio2) {
		pXAudio2->Release();
		pXAudio2 = NULL;
	}

	EnterCriticalSection(&audSection);
	audio_shutdown();
	LeaveCriticalSection(&audSection);
	DeleteCriticalSection(&audSection);
}

/*  CXAudio2::DeInitVoices
deinitializes the voice objects and buffers and sets the mixing thread in wait state
*/
void CXAudio2::DeInitVoices(void)
{

	if(pSourceVoice) {
		StopPlayback();		 
		pSourceVoice->DestroyVoice();
		pSourceVoice = NULL;
	}
	if(pMasteringVoice) {
		pMasteringVoice->DestroyVoice();
		pMasteringVoice = NULL;
	}

	if(pAudioBuffers) {
		free(pAudioBuffers);
		pAudioBuffers = NULL;
	}

	if(m_mixbuf) {
		delete [] m_mixbuf;
		m_mixbuf = NULL;
	}
 
 
}

 

/*  CXAudio2::PushBuffer
pushes one buffer onto the source voice playback queue
IN:
AudioBytes		-	size of the buffer
pAudioData		-	pointer to the buffer
pContext		-	context passed to the callback, unused
*/
void CXAudio2::PushBuffer(UINT32 AudioBytes,BYTE *pAudioData,void *pContext)
{
	XAUDIO2_BUFFER xa2buffer={0};
	xa2buffer.AudioBytes=AudioBytes;
	xa2buffer.pAudioData=pAudioData;
	xa2buffer.pContext=pContext;
	pSourceVoice->SubmitSourceBuffer(&xa2buffer);
}

/*  CXAudio2::SetupSound
applies current sound settings by recreating the voice objects and buffers
IN/OUT:
syncSoundBuffer		-	will point to the temp buffer that can be used for SoundSync
sample_count		-	number of samples that fit into syncSoundBuffer
-----
returns true if successful, false otherwise
*/
void CXAudio2::OnBufferEnd(void *pBufferContext)
{
	InterlockedIncrement(&emptyBuffers);		// we are possibly decrementing on another thread
	SetEvent(eventHandle);						// signal mixing thread that a new buffer can be filled
}

bool CXAudio2::SetupSound()
{

    nXAudio2Fps = 60*100; //Number of FPS * 100
    cbLoopLen = 0;  // Loop length (in bytes) calculated
    currentBuffer = 0;
    pAudioBuffers = NULL;
    nAudSegCount = 6;

    nAudSegLen = (SAMPLERATE * 100 + (nXAudio2Fps >> 1)) / nXAudio2Fps;
    nAudAllocSegLen = nAudSegLen << 2;
    cbLoopLen = (nAudSegLen * nAudSegCount) << 2;
 
    // Allocate sound Buffer	 
    m_mixbuf = new short[nAudAllocSegLen];
    pAudioBuffers = (BYTE *)malloc(cbLoopLen);
	memset(m_mixbuf,0x00, nAudAllocSegLen);
	memset(pAudioBuffers,0x00,cbLoopLen);

	pXAudio2->CreateMasteringVoice( &pMasteringVoice );
    wfx.wBitsPerSample      = 16;
    wfx.wFormatTag                              = WAVE_FORMAT_PCM;
    wfx.nChannels                  = 2;
    wfx.nSamplesPerSec     = SAMPLERATE;
    wfx.nBlockAlign                                = wfx.wBitsPerSample * wfx.nChannels /8;
    wfx.nAvgBytesPerSec   = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

	HRESULT hr = pXAudio2->CreateSourceVoice( &pSourceVoice, ( WAVEFORMATEX* )&wfx , XAUDIO2_VOICE_USEFILTER , XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL ) ;
	
    pSourceVoice->SetVolume( 1.0 );
	pSourceVoice->FlushSourceBuffers();
  
	
	BeginPlayback();
	

	
	return true;

}

void CXAudio2::BeginPlayback()
{
	pSourceVoice->Start(0);
}

void CXAudio2::StopPlayback()
{
	pSourceVoice->Stop(0);
}

/*  CXAudio2::ProcessSound
the mixing function called by the mix thread
called once per played buffer (when the callback signals the thread)
synchronizes the syncSoundBuffer access with a critical section (if SoundSync is enabled)
*/
void CXAudio2::ProcessSound()
{
	 XAUDIO2_VOICE_STATE                               vState;

	if (!pSourceVoice)
		return;

   //while(emptyBuffers) {
 

   for (int j=0;j<nAudAllocSegLen>>1;j++)
   {
		m_mixbuf[j*2+0]=snd.buffer[0][j]; //Snd L buffer  from the emu
		m_mixbuf[j*2+1]=snd.buffer[1][j]; //Snd R buffer  from the emu
   } 

    while (true) {
                                pSourceVoice->GetState(&vState);
 
                          
                                if (vState.BuffersQueued < nAudSegCount - 1) {
                                                if (vState.BuffersQueued == 0) {
                                                                // buffers ran dry
                                                }
                                                // there is at least one free buffer
                                                break;
                                }
                }
   
    // copy & protect the audio data in own memory area while playing it
   
	memcpy(&pAudioBuffers[currentBuffer * nAudAllocSegLen], m_mixbuf, nAudAllocSegLen);	

	PushBuffer(nAudAllocSegLen, &pAudioBuffers[currentBuffer * nAudAllocSegLen], NULL);
	
    currentBuffer++;
    currentBuffer %= (nAudSegCount);

 
	 //}
}
 

/*  CXAudio2::SoundThread
this is the actual mixing thread, runs until DeInitXAudio2 sets exitThread
since this is a new thread we need CoInitialzeEx to allow XAudio2 access
IN:
lpParameter		-	pointer to the CXAudio2 object
-----
returns true if successful, false otherwise
*/
unsigned int __stdcall CXAudio2::SoundThread (LPVOID lpParameter)
{

	XSetThreadProcessor( GetCurrentThread(), 2 );

	CXAudio2 *XAudio2=(CXAudio2 *)lpParameter;
#ifndef _XBOX
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
#endif
	while(1) {
		WaitForSingleObject(XAudio2->eventHandle,INFINITE);
		if(XAudio2->exitThread) {
#ifndef _XBOX
			CoUninitialize();
#endif
			_endthreadex(0);
		}
		XAudio2->ProcessSound();
	}
	return 0;
}