#pragma once

#include <Windows.h>
#include <cstdio>

struct timer_t {
	double cpu_freq;	// in kHz
	__int64 counter_start;

	__int64 get() const {
		LARGE_INTEGER li;
		QueryPerformanceCounter(&li);
		return li.QuadPart;
	}
public:
	bool init() {
		LARGE_INTEGER li;
		QueryPerformanceFrequency(&li);
		cpu_freq = double(li.QuadPart);	// in Hz. this is subject to dynamic frequency scaling, though
		begin();
		return true;
	}
	void begin() {
		LARGE_INTEGER li;
		QueryPerformanceFrequency(&li);
		cpu_freq = double(li.QuadPart);
		QueryPerformanceCounter(&li);
		counter_start = li.QuadPart;
	}


	inline double get_s() const {
		return (double)(get() - counter_start) / cpu_freq;
	}
	inline double get_ms() const {
		return double(1000 * (get_s()));
	}
	inline double get_us() const {
		return double(1000000 * (get_s()));
	}
	timer_t() {
		if (!init()) { printf("timer_t: error: initialization failed.\n"); }
	}
};