#pragma once

#include <Windows.h>

#pragma comment(lib, "Avrt.lib")

HRESULT PlayAudioStream();

struct wave_format_t {
	int num_channels;
	int sample_rate;
	int bit_depth;
	float wave_freq;
	float cycle_duration_ms;
};

UINT32 SND_get_frame_size();
wave_format_t SND_get_format_info();
int SND_initialized();
size_t SND_write_to_buffer(const float *data);
