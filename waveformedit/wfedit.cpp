#include "wfedit.h"
#include "glwindow.h"
#include "lin_alg.h"
#include "sound.h"
#include "curve.h"
#include "timer.h"

#include <cstdio>
#include <iostream>
#include <cassert>

static int program_running = 1;

static DWORD WINAPI sound_thread_proc(LPVOID lpParam) {
	return (DWORD)PlayAudioStream();
}

int wfedit_running() {
	return program_running;
}

static void wfedit_stop() {
	program_running = 0;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

#define ENABLE_CONSOLE

#ifdef ENABLE_CONSOLE
	if (AllocConsole()) {
		// for debugging those early-program fatal erreurz. this will screw up our framerate though.
		FILE *dummy;
		freopen_s(&dummy, "CONOUT$", "wt", stdout);

		SetConsoleTitle("debug output");
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED);
	}
#endif


	long wait = 0;
	static double time_per_frame_ms = 0;

	MSG msg;

//	BEZIER4 B(vec2(0.0, 5.0), vec2(0.42, -5.0), vec2(0.67, 2.0), vec2(1.0, 0.33));
//	CATMULLROM4 C(vec2(0.0, 0.0), vec2(0.3, 1.0), vec2(0.6, -1.0), vec2(1.0, 0.0), 1);
//
//#define NUM_ITERATIONS 250
//	
//	float dt = 1.0 / NUM_ITERATIONS;
//
//	for (int i = 0; i < NUM_ITERATIONS; ++i) {
//		float t = (float)i * dt;
//		vec2 v = C.evaluate(t);
//
//		printf("(%f, %f)\n", v.x, v.y);
//	}
//
//
	
	DWORD sound_threadID;
	CreateThread(NULL, 0, sound_thread_proc, NULL, 0, &sound_threadID);

	if (!create_GL_window("WFEDIT", WIN_W, WIN_H)) {
		return EXIT_FAILURE;
	}

	if (!init_GL()) {
		return EXIT_FAILURE;
	}

	bool running = true;


	while (wfedit_running()) {
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0) {
			if (msg.message == WM_QUIT) {
				wfedit_stop();
				break;
			}
			else {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		draw();
		swap_buffers();
	}


	return (msg.wParam);
}
