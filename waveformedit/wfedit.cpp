#include "wfedit.h"
#include "glwindow.h"
#include "lin_alg.h"
#include "sound.h"

#include <cstdio>
#include <iostream>

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
