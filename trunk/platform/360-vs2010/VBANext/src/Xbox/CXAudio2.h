/* CXAudio2.h - written by OV2 */

#ifndef CXHAUDIO2_H
#define CXAUDIO2_H
#include "XAudio2.h"
#include "types.h"
#include <xtl.h>
 

class CXAudio2 : public IXAudio2VoiceCallback 
{
private:
 
	
	HANDLE threadHandle;

	bool initDone;							// has init been called successfully?

	XAUDIO2_VOICE_STATE                               vState;
	IXAudio2*                                               pXAudio2;
	IXAudio2MasteringVoice* pMasteringVoice;
	
	WAVEFORMATEX    wfx;

	//SOund TEST
	signed short                       *m_mixbuf ;
	BYTE* pAudioBuffers;
	int currentBuffer;

	int cbLoopLen;                                                 
	int nXAudio2Fps;             
	
	int nAudSegCount;
	

	volatile bool exitThread;				// switch to exit the thread 
	
	volatile LONG emptyBuffers;	
 

	void PushBuffer(UINT32 AudioBytes,BYTE *pAudioData,void *pContext);	

	
	bool InitXAudio2(void);
	void DeInitXAudio2(void);

	static unsigned int __stdcall SoundThread (LPVOID lpParameter);

	CRITICAL_SECTION audSection;

public:
	CXAudio2(void);
	~CXAudio2(void);
		
	// inherited from IXAudio2VoiceCallback - we only use OnBufferEnd
	STDMETHODIMP_(void) OnBufferEnd(void *pBufferContext);
	STDMETHODIMP_(void) OnBufferStart(void *pBufferContext){}
	STDMETHODIMP_(void) OnLoopEnd(void *pBufferContext){}
	STDMETHODIMP_(void) OnStreamEnd() {}
	STDMETHODIMP_(void) OnVoiceError(void *pBufferContext, HRESULT Error) {}
	STDMETHODIMP_(void) OnVoiceProcessingPassEnd() {}
	STDMETHODIMP_(void) OnVoiceProcessingPassStart(UINT32 BytesRequired) {}

	HANDLE eventHandle;
	HANDLE pauseHandle;

	bool InitVoices(void);
	void DeInitVoices(void);

	void BeginPlayback(void);
	void StopPlayback(void);

	// Inherited from IS9xSoundOutput
	bool InitSoundOutput(void) { return InitXAudio2(); }
	void DeInitSoundOutput(void) { DeInitXAudio2(); }
	bool SetupSound();
	void ProcessSound(void);

	IXAudio2SourceVoice*    pSourceVoice;

	int nAudAllocSegLen;
	int nAudSegLen;
};

#endif