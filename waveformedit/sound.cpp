#include "sound.h"

#include <Audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <ksmedia.h>
#include <stdio.h>
#include <cmath>
#include <limits>
#include <Avrt.h>

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
			printf("sound.cpp:%d: WASAPI code error %lX\n", __LINE__, hr);\
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

static inline float sin01(float alpha) {
	return 0.5*sin(alpha) + 0.5;
}

static inline float sin_minmax_Hz(float min, float max, float Hz, float t) {
	return (max - min) / 2.0 * sin01(t * Hz * 6.28318531);
}

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
	HANDLE hEvent;
	HANDLE hTask = NULL;


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

	IF_ERROR_EXIT(hr);

	REFERENCE_TIME DefaultDevicePeriod = 0;
	REFERENCE_TIME MinimumDevicePeriod = 0;
	hr = pAudioClient->GetDevicePeriod(&DefaultDevicePeriod, &MinimumDevicePeriod);
	
	IF_ERROR_EXIT(hr);

	WAVEFORMATEX wave_format = {};
	wave_format.wFormatTag = WAVE_FORMAT_PCM;
	wave_format.nChannels = 2;
	wave_format.nSamplesPerSec = 44100;
	wave_format.nAvgBytesPerSec = 44100 * 2 * 16 / 8;
	wave_format.nBlockAlign = 2 * 16 / 8;
	wave_format.wBitsPerSample = 16;

	hr = pAudioClient->IsFormatSupported(
		AUDCLNT_SHAREMODE_EXCLUSIVE,
		&wave_format,
		NULL // can't suggest a "closest match" in exclusive mode
		);
	if (AUDCLNT_E_UNSUPPORTED_FORMAT == hr) {
		printf("WASAPI: default audio device does not support the requested WAVEFORMATEX (44100/2ch/16bit, aka CD)\n");
		pAudioClient->Release();
		return hr;
	}
	
	IF_ERROR_EXIT(hr);

	hr = pAudioClient->Initialize(
		AUDCLNT_SHAREMODE_EXCLUSIVE,
		AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		DefaultDevicePeriod,
		DefaultDevicePeriod,
		&wave_format,
		NULL);
	IF_ERROR_EXIT(hr);


	// Get the actual size of the allocated buffer.
	hr = pAudioClient->GetBufferSize(&bufferFrameCount);

	INT32 FrameSize_bytes = bufferFrameCount * wave_format.nChannels * wave_format.wBitsPerSample / 8;

	IF_ERROR_EXIT(hr);

	hr = pAudioClient->GetService(
		IID_IAudioRenderClient,
		(void**)&pRenderClient);
	IF_ERROR_EXIT(hr);

	hEvent = CreateEvent(nullptr, false, false, nullptr);
	if (hEvent == INVALID_HANDLE_VALUE) { printf("CreateEvent failed\n");  return -1; }

	hr = pAudioClient->SetEventHandle(hEvent);
	IF_ERROR_EXIT(hr);

	const size_t num_samples = FrameSize_bytes / sizeof(unsigned short);

	unsigned short *samples = new unsigned short[num_samples];

	float max = (float)(std::numeric_limits<unsigned short>::max)();
	float halfmax = max / 2.0;
	float sample_rate_recip = 1.0 / (float)wave_format.nSamplesPerSec;

	for (int i = 0; i < num_samples; ) {
		float t = (float)i / (float) wave_format.nSamplesPerSec;
		samples[i] = sin_minmax_Hz(0, max, 440, t);
		samples[i + 1] = sin_minmax_Hz(0, max, 440, t);
		i += 2;
	}

	hr = pRenderClient->GetBuffer(bufferFrameCount, &pData);
	IF_ERROR_EXIT(hr);

	memcpy(pData, samples, FrameSize_bytes);

	hr = pRenderClient->ReleaseBuffer(bufferFrameCount, flags);
	IF_ERROR_EXIT(hr);

	// Calculate the actual duration of the allocated buffer.
	hnsActualDuration = (double)REFTIMES_PER_SEC *
		bufferFrameCount / wave_format.nSamplesPerSec;

	printf("bufferFrameCount: %d, FrameSize_bytes: %d\n", bufferFrameCount, FrameSize_bytes);

	// this should increase thread priority to ensure low latency :D
	DWORD taskIndex = 0;
	hTask = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
	if (hTask == NULL) {
		hr = E_FAIL;
		IF_ERROR_EXIT(hr);
	}

	hr = pAudioClient->Start();  // Start playing.
	IF_ERROR_EXIT(hr);

	while (true) {

		WaitForSingleObject(hEvent, INFINITE);

		hr = pRenderClient->GetBuffer(bufferFrameCount, &pData);
		IF_ERROR_EXIT(hr);

		//if (numFramesAvailable == 0) break;

		memcpy(pData, samples, FrameSize_bytes);

		hr = pRenderClient->ReleaseBuffer(bufferFrameCount, flags);
		IF_ERROR_EXIT(hr);
	}

	// Wait for last data in buffer to play before stopping.
	Sleep((DWORD)(hnsActualDuration / REFTIMES_PER_MILLISEC / 2));

	hr = pAudioClient->Stop();  // Stop playing.
	IF_ERROR_EXIT(hr);

	SAFE_RELEASE(pEnumerator)
	SAFE_RELEASE(pDevice)
	SAFE_RELEASE(pAudioClient)
	SAFE_RELEASE(pRenderClient)

	if (hEvent != NULL) {
		CloseHandle(hEvent);
	}
	
	if (hTask != NULL) {
		AvRevertMmThreadCharacteristics(hTask);
	}

	return hr;
}
