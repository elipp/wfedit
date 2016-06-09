#include "glwindow.h"
#include "lin_alg.h"
#include "sound.h"

#include <cstdio>
#include <iostream>


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{

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

	static float running = 0;
	MSG msg;
	
	if (!create_GL_window("WFEDIT", WIN_W, WIN_H)) {
		return EXIT_FAILURE;
	}

	if (!init_GL()) {
		return EXIT_FAILURE;
	}

	bool done = false;

	PlayAudioStream();

	while (!done) {
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0) {
			if (msg.message == WM_QUIT) {
				return EXIT_SUCCESS;
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
