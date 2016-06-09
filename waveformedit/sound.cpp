#include "sound.h"

#include <Audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <ksmedia.h>
#include <stdio.h>
#include <cmath>

//-----------------------------------------------------------
// Play an audio stream on the default audio rendering
// device. The PlayAudioStream function allocates a shared
// buffer big enough to hold one second of PCM audio data.
// The function uses this buffer to stream data to the
// rendering device. The inner loop runs every 1/2 second.
//-----------------------------------------------------------

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#define IF_ERROR_EXIT(hr) do {\
		if (FAILED(hr)){\
			printf("sound.cpp:%d: WASAPI code error %lu\n", __LINE__, hr);\
			return -1;\
		}\
} while(0)

#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

HRESULT PlayAudioStream() {

	HRESULT hr;
	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
	REFERENCE_TIME hnsActualDuration;
	IMMDeviceEnumerator *pEnumerator = NULL;
	IMMDevice *pDevice = NULL;
	IAudioClient *pAudioClient = NULL;
	IAudioRenderClient *pRenderClient = NULL;
	WAVEFORMATEX *pwfx = NULL;
	UINT32 bufferFrameCount;
	UINT32 numFramesAvailable;
	UINT32 numFramesPadding;
	BYTE *pData;
	DWORD flags = 0;

	return 0;

	hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);

	hr = pEnumerator->GetDefaultAudioEndpoint(
		eRender, eConsole, &pDevice);
	IF_ERROR_EXIT(hr);

	hr = pDevice->Activate(
		IID_IAudioClient, CLSCTX_ALL,
		NULL, (void**)&pAudioClient);
	IF_ERROR_EXIT(hr);

	hr = pAudioClient->GetMixFormat(&pwfx);

	WAVEFORMATEXTENSIBLE* w = (WAVEFORMATEXTENSIBLE*)pwfx;

	printf("pwfx: %X, %d, %d, %d\n", w->Format.wFormatTag, w->Format.nSamplesPerSec, w->Format.wBitsPerSample, w->SubFormat);

	IF_ERROR_EXIT(hr);

	hr = pAudioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		0,
		hnsRequestedDuration,
		0,
		pwfx,
		NULL);
	IF_ERROR_EXIT(hr);

	// Tell the audio source which format to use.
	//hr = pMySource->SetFormat(pwfx);
	IF_ERROR_EXIT(hr);

	// Get the actual size of the allocated buffer.
	hr = pAudioClient->GetBufferSize(&bufferFrameCount);
	IF_ERROR_EXIT(hr);

	hr = pAudioClient->GetService(
		IID_IAudioRenderClient,
		(void**)&pRenderClient);
	IF_ERROR_EXIT(hr);

	float *samples = new float[bufferFrameCount];

	for (int i = 0; i < bufferFrameCount; ++i) {
		samples[i] = sin(400 * (i / 3.14));
	}

	// Grab the entire buffer for the initial fill operation.
	hr = pRenderClient->GetBuffer(bufferFrameCount, (BYTE**)samples);
	IF_ERROR_EXIT(hr);

	// Load the initial data into the shared buffer.
	//hr = pMySource->LoadData(bufferFrameCount, pData, &flags);

	
	IF_ERROR_EXIT(hr);

	hr = pRenderClient->ReleaseBuffer(bufferFrameCount, flags);
	IF_ERROR_EXIT(hr);

	// Calculate the actual duration of the allocated buffer.
	hnsActualDuration = (double)REFTIMES_PER_SEC *
		bufferFrameCount / pwfx->nSamplesPerSec;

	printf("hnsActualDuration = %f\n", hnsActualDuration);

	hr = pAudioClient->Start();  // Start playing.
	IF_ERROR_EXIT(hr);

	// Each loop fills about half of the shared buffer.
	while (flags != AUDCLNT_BUFFERFLAGS_SILENT) {
		// Sleep for half the buffer duration.
		Sleep((DWORD)(hnsActualDuration / REFTIMES_PER_MILLISEC / 2));

		// See how much buffer space is available.
		hr = pAudioClient->GetCurrentPadding(&numFramesPadding);
		IF_ERROR_EXIT(hr);

		numFramesAvailable = bufferFrameCount - numFramesPadding;

		// Grab all the available space in the shared buffer.
		hr = pRenderClient->GetBuffer(numFramesAvailable, &pData);
		IF_ERROR_EXIT(hr);

		// Get next 1/2-second of data from the audio source.
	//	hr = pMySource->LoadData(numFramesAvailable, pData, &flags);
		IF_ERROR_EXIT(hr);

		hr = pRenderClient->ReleaseBuffer(numFramesAvailable, flags);
		IF_ERROR_EXIT(hr);
	}

	// Wait for last data in buffer to play before stopping.
	Sleep((DWORD)(hnsActualDuration / REFTIMES_PER_MILLISEC / 2));

	hr = pAudioClient->Stop();  // Stop playing.
	IF_ERROR_EXIT(hr);

//Exit:
	CoTaskMemFree(pwfx);
	SAFE_RELEASE(pEnumerator)
		SAFE_RELEASE(pDevice)
		SAFE_RELEASE(pAudioClient)
		SAFE_RELEASE(pRenderClient)

		return hr;
}
