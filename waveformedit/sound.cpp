#include "sound.h"

#include <Audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <ksmedia.h>
#include <stdio.h>
#include <cmath>
#include <limits>
#include <Avrt.h>

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#define IF_ERROR_EXIT(hr) do {\
		if (FAILED(hr)){\
			printf("sound.cpp:%d: WASAPI code error %lX\n", __LINE__, hr);\
			goto exit_err;\
		}\
} while(0)

#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

#define TWO_PI (3.14159265359*2)

static inline float sin01(float alpha) {
	return 0.5*sin(alpha) + 0.5;
}

static inline float sin_minmax_Hz(float min, float max, float freq_Hz, float t) {
	return (max - min) / 2.0 * sin01(t * freq_Hz * TWO_PI);
}

static HRESULT find_smallest_128_aligned(IMMDevice *pDevice, IAudioClient *pAudioClient, const WAVEFORMATEX *wave_format) {

	REFERENCE_TIME DefaultDevicePeriod = 0, MinimumDevicePeriod = 0;
	HRESULT hr = pAudioClient->GetDevicePeriod(&DefaultDevicePeriod, &MinimumDevicePeriod);

	if (FAILED(hr)) return hr;

	printf("MinimumDevicePeriod: %lld, DefaultDevicePeriod: %lld\n", MinimumDevicePeriod, DefaultDevicePeriod);

	int n = 128; 

	while (n < 16384) {
		
		REFERENCE_TIME hnsPeriod = (REFERENCE_TIME)((float)REFTIMES_PER_SEC * (float)n / (float)wave_format->nSamplesPerSec + 0.5);

		hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, hnsPeriod, hnsPeriod, wave_format, NULL);

		if (FAILED(hr)) {
			if (hr == AUDCLNT_E_INVALID_DEVICE_PERIOD) {
				printf("IAudioClient::Initialize() returned AUDCLNT_E_INVALID_DEVICE_PERIOD for n = %d (-> %lld), trying again...\n", n, hnsPeriod);
				hr = pAudioClient->Release(); // release so we can initialize() again
				if (FAILED(hr)) return hr;

				hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
				if (FAILED(hr)) return hr;
				
			}
			else {
				return hr;
			}
		}
		else {
			printf("IAudioClient::Initialize(): success with n = %d (period = %lld === %.4f ms)\n", n, hnsPeriod, (float)hnsPeriod / 10000.0);
			return hr;
		}
	
		n += 32;

	}

	return AUDCLNT_E_INVALID_DEVICE_PERIOD; // this is bad.. but w/e
}

HRESULT PlayAudioStream() {

	HRESULT hr;
	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
	IMMDeviceEnumerator *pEnumerator = NULL;
	IMMDevice *pDevice = NULL;
	IAudioClient *pAudioClient = NULL;
	IAudioRenderClient *pRenderClient = NULL;
	UINT32 bufferFrameCount = 0;
	WAVEFORMATEX *pwfx = NULL;
	BYTE *pData = NULL;
	DWORD flags = 0;
	HANDLE hEvent = NULL;
	HANDLE hTask = NULL;

	WAVEFORMATEX wave_format = {};

	hr = CoInitialize(NULL);
	IF_ERROR_EXIT(hr);

	hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator);
	IF_ERROR_EXIT(hr);

	hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
	IF_ERROR_EXIT(hr);

	hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
	IF_ERROR_EXIT(hr);


	wave_format.wFormatTag = WAVE_FORMAT_PCM;
	wave_format.nChannels = 2;
	wave_format.nSamplesPerSec = 96000;
	wave_format.nAvgBytesPerSec = 96000 * 2 * 16 / 8;
	wave_format.nBlockAlign = 2 * 16 / 8;
	wave_format.wBitsPerSample = 16;

	hr = pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &wave_format, NULL);

	if (AUDCLNT_E_UNSUPPORTED_FORMAT == hr) {
		printf("WASAPI: default audio device does not support the requested WAVEFORMATEX (%d/%dch/%d bit)\n", wave_format.nSamplesPerSec, wave_format.nChannels, wave_format.wBitsPerSample);
		pAudioClient->Release();
		return hr;
	}
	IF_ERROR_EXIT(hr);

	hr = find_smallest_128_aligned(pDevice, pAudioClient, &wave_format);
	IF_ERROR_EXIT(hr);
	
	hr = pAudioClient->GetBufferSize(&bufferFrameCount);
	IF_ERROR_EXIT(hr);

	INT32 FrameSize_bytes = bufferFrameCount * wave_format.nChannels * wave_format.wBitsPerSample / 8;

	hr = pAudioClient->GetService(IID_IAudioRenderClient, (void**)&pRenderClient);
	IF_ERROR_EXIT(hr);

	hEvent = CreateEvent(nullptr, false, false, nullptr);
	if (hEvent == INVALID_HANDLE_VALUE) { printf("CreateEvent failed\n");  return -1; }
	
	hr = pAudioClient->SetEventHandle(hEvent);
	IF_ERROR_EXIT(hr);

	const size_t num_samples = FrameSize_bytes / sizeof(unsigned short);

	unsigned short *samples = new unsigned short[num_samples];

	float min = (float)(std::numeric_limits<unsigned short>::min)();
	float max = (float)(std::numeric_limits<unsigned short>::max)();
	float halfmax = max / 2.0;
	float dt = 1.0 / (float)wave_format.nSamplesPerSec;

	float freq = 2*(float)wave_format.nSamplesPerSec / (float)bufferFrameCount;
	//float freq = 440;

	for (int i = 0; i < num_samples/2; ++i) {
		float t = (float)i * dt;
		samples[2*i] = sin_minmax_Hz(min, max, freq, t); // L channel
		samples[2*i + 1] = sin_minmax_Hz(min, max, freq, t); // R channel
	}

	hr = pRenderClient->GetBuffer(bufferFrameCount, &pData);
	IF_ERROR_EXIT(hr);

	memcpy(pData, samples, FrameSize_bytes);

	hr = pRenderClient->ReleaseBuffer(bufferFrameCount, flags);
	IF_ERROR_EXIT(hr);


	printf("bufferFrameCount: %d, FrameSize_bytes: %d\n", bufferFrameCount, FrameSize_bytes);

	// increase thread priority for optimal av performance
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

		memcpy(pData, samples, FrameSize_bytes);

		hr = pRenderClient->ReleaseBuffer(bufferFrameCount, 0);
		IF_ERROR_EXIT(hr);
	}


	hr = pAudioClient->Stop();  // Stop playing.
	IF_ERROR_EXIT(hr);

exit_err:

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
