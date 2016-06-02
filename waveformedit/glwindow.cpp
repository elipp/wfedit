#include "glwindow.h"

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>
#include <stdarg.h>

#include <cassert>
#include <signal.h>
#include <mutex>

#include "texture.h"
#include "shader.h"
#include "lin_alg.h"

bool mouse_locked = false;

static HGLRC hRC = NULL;
static HDC hDC = NULL;
static HWND hWnd = NULL;
static HINSTANCE hInstance;

extern HINSTANCE win_hInstance;

static HWND hWnd_child = NULL;

unsigned WINDOW_WIDTH = 1280;
unsigned WINDOW_HEIGHT = 960;

static GLuint wave_VBOid, VAOid;

bool fullscreen = false;
bool active = TRUE;

static Texture *gradient_texture;
static ShaderProgram *wave_shader;

static bool _main_loop_running = true;
bool main_loop_running() { return _main_loop_running; }
void stop_main_loop() { _main_loop_running = false; }

void kill_GL_window();

void set_cursor_relative_pos(int x, int y) {
	POINT pt;
	pt.x = x;
	pt.y = y;
	ClientToScreen(hWnd, &pt);
	SetCursorPos(pt.x, pt.y);
}

static std::string *convertLF_to_CRLF(const char *buf);

void swap_buffers() {
	SwapBuffers(hDC);
}

int generate_vertices(float *samples, size_t num_samples) {

	static const float dx = 1.0;
	static const float w = 0.5;
	float dy = samples[1] - samples[0];

	float x = 0;

	float k = dy / dx;

	float alpha = atan(k);

	vec4 a(w*sin(alpha), -w*cos(alpha), 0, 0);
	vec4 orig_p(x, samples[0], 0, 0);

	vec4 p1 = orig_p + a;
	vec4 p2 = orig_p - a;

	return 0;
}

void draw() {

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(wave_shader->getProgramHandle());

	mat4 mvp = mat4::proj_ortho(0.0, WIN_W, WIN_H, 0.0, -1.0, 1.0) * mat4::translate(300, 300, 0) * mat4::scale(100, 100, 100);
	wave_shader->update_uniform_mat4("uMVP", mvp);

//	glBindBuffer(GL_ARRAY_BUFFER, wave_VBOid);
	//glVertexAttribPointer(ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
	glBindVertexArray(VAOid);	
	glDrawArrays(GL_PATCHES, 0, 4);

}

int init_GL() {

	if (!load_GL_extensions()) {
		return 0;
	}

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glEnable(GL_DEPTH_TEST);

	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//glEnable(GL_BLEND);

	glViewport(0, 0, WIN_W, WIN_H);

	const char* version = (const char*)glGetString(GL_VERSION);

	printf("OpenGL version information:\n%s\n\n", version);

	GLint max_elements_vertices, max_elements_indices;
	glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &max_elements_vertices);
	glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &max_elements_indices);

	glPatchParameteri(GL_PATCH_VERTICES, 4);

	printf("GL_MAX_ELEMENTS_VERTICES = %d\nGL_MAX_ELEMENTS_INDICES = %d\n", max_elements_vertices, max_elements_indices);


#define ADD_ATTRIB(map, attrib_id, attrib_name) do {\
	(map).insert(std::make_pair<GLuint, std::string>((attrib_id), std::string(attrib_name)));\
	} while(0)

	std::unordered_map<GLuint, std::string> default_attrib_bindings;
	ADD_ATTRIB(default_attrib_bindings, ATTRIB_POSITION, "Position_VS_in");

	wave_shader = new ShaderProgram("shaders/wave", default_attrib_bindings);

	float patch_buffer[4 * 2] = { 0.0, 0.0, 1.0, 1.0, 10, 0.0, 7.0, 5 };

	glGenVertexArrays(1, &VAOid);
	glBindVertexArray(VAOid);

	glEnableVertexAttribArray(ATTRIB_POSITION);

	glGenBuffers(1, &wave_VBOid);
	glBindBuffer(GL_ARRAY_BUFFER, wave_VBOid);
	glBufferData(GL_ARRAY_BUFFER, 8*sizeof(float), patch_buffer, GL_DYNAMIC_DRAW);

	glVertexAttribPointer(ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), 0);

	glBindVertexArray(0);

	//glDisableVertexAttribArray(ATTRIB_POSITION);

	return 1;


}


LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
#define BIT_SET(var,pos) ((var) & (1<<(pos)))
	switch (uMsg)
	{
	case WM_ACTIVATE:
		if (!HIWORD(wParam)) { active = TRUE; }
		else {
			active = FALSE;
			mouse_locked = false;
		}
		break;

	case WM_SYSCOMMAND:
		switch (wParam)
		{
		case SC_SCREENSAVE:
		case SC_KEYMENU:
			if ((lParam >> 16) <= 0) return 0;
			else return DefWindowProc(hWnd, uMsg, wParam, lParam);
		case SC_MONITORPOWER:
			return 0;
		}
		break;

	case WM_CLOSE:
		kill_GL_window();
		PostQuitMessage(0);
		break;

	case WM_CHAR:
		//handle_char_input(wParam);
		break;

	case WM_KEYDOWN:
		//handle_key_press(wParam);
		break;

	case WM_KEYUP:
		//keys[wParam] = false;
		break;

	case WM_SIZE:
		//resize_GL_scene(LOWORD(lParam), HIWORD(lParam));
		break;

	default:
		break;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


void messagebox_error(const std::string &msg) {
	MessageBox(NULL, msg.c_str(), "Error (fatal)", MB_OK | MB_ICONEXCLAMATION);
}

int create_GL_window(const char* title, int width, int height) {
	hInstance = GetModuleHandle(NULL);

	GLuint PixelFormat;
	WNDCLASS wc;
	DWORD dwExStyle;
	DWORD dwStyle;

	RECT WindowRect;
	WindowRect.left = (long)0;
	WindowRect.right = (long)width;
	WindowRect.top = (long)0;
	WindowRect.bottom = (long)height;


	fullscreen = false;

	//hInstance = GetModuleHandle(NULL);
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = (WNDPROC)WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "OpenGL";

	if (!RegisterClass(&wc))
	{
		MessageBox(NULL, "FAILED TO REGISTER THE WINDOW CLASS.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}

	DEVMODE dmScreenSettings;
	memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
	dmScreenSettings.dmSize = sizeof(dmScreenSettings);
	dmScreenSettings.dmPelsWidth = width;
	dmScreenSettings.dmPelsHeight = height;
	dmScreenSettings.dmBitsPerPel = 32;
	dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

	/*
	* no need to test this now that fullscreen is turned off
	*
	if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
	{
	if (MessageBox(NULL, "The requested fullscreen mode is not supported by\nyour video card. Use Windowed mode instead?", "warn", MB_YESNO | MB_ICONEXCLAMATION)==IDYES)
	{
	fullscreen=FALSE;
	}
	else {

	MessageBox(NULL, "Program willl now close.", "ERROR", MB_OK|MB_ICONSTOP);
	return FALSE;
	}
	}*/

	if (fullscreen)
	{
		dwExStyle = WS_EX_APPWINDOW;
		dwStyle = WS_POPUP;

	}

	else {
		dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
		dwStyle = WS_OVERLAPPEDWINDOW;
	}

	AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle);

	hWnd = CreateWindowEx(dwExStyle, "OpenGL", title, WS_CLIPSIBLINGS | WS_CLIPCHILDREN | dwStyle, 0, 0,
		WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top,
		NULL, NULL, hInstance, NULL);

	if (hWnd == NULL) {
		MessageBox(NULL, "window creation error.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}


	static PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA,
		32,
		0, 0, 0, 0, 0, 0,
		0,
		0,
		0,
		0, 0, 0, 0,
		24,
		0,
		0,
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};

	if (!(hDC = GetDC(hWnd)))
	{
		MessageBox(NULL, "CANT CREATE A GL DEVICE CONTEXT.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}

	if (!(PixelFormat = ChoosePixelFormat(hDC, &pfd)))
	{
		MessageBox(NULL, "cant find a suitable pixelformat.", "ERROUE", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}


	if (!SetPixelFormat(hDC, PixelFormat, &pfd))
	{
		MessageBox(NULL, "Can't SET ZE PIXEL FORMAT.", "ERROU", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}

	if (!(hRC = wglCreateContext(hDC)))
	{
		MessageBox(NULL, "WGLCREATECONTEXT FAILED.", "ERREUHX", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}

	if (!wglMakeCurrent(hDC, hRC))
	{
		MessageBox(NULL, "Can't activate the gl rendering context.", "ERAIX", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}

	// apparently, the WINAPI ShowWindow calls some opengl functions and causes a crash if the funcptrs aren't loaded before hand

	//	wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress ("wglSwapIntervalEXT");  

	if (!load_GL_extensions()) {
		return FALSE;
	}


	ShowWindow(hWnd, SW_SHOWDEFAULT);
	//SetForegroundWindow(hWnd);
	//SetFocus(hWnd);

	return TRUE;
}

void kill_GL_window() {

	if (hRC) {
		if (!wglMakeCurrent(NULL, NULL)) {
			MessageBox(NULL, "wglMakeCurrent(NULL,NULL) failed", "erreur", MB_OK | MB_ICONINFORMATION);
		}

		if (!wglDeleteContext(hRC)) {
			MessageBox(NULL, "RELEASE of rendering context failed.", "error", MB_OK | MB_ICONINFORMATION);
		}

		hRC = NULL;

		if (hDC && !ReleaseDC(hWnd, hDC))
		{
			MessageBox(NULL, "Release DC failed.", "ERREUX", MB_OK | MB_ICONINFORMATION);
			hDC = NULL;
		}

		if (hWnd && !DestroyWindow(hWnd))
		{
			MessageBox(NULL, "couldn't release hWnd.", "erruexz", MB_OK | MB_ICONINFORMATION);
			hWnd = NULL;
		}

		if (!UnregisterClass("OpenGL", hInstance))
		{
			MessageBox(NULL, "couldn't unregister class.", "err", MB_OK | MB_ICONINFORMATION);
			hInstance = NULL;
		}

	}

}

static std::string *convertLF_to_CRLF(const char *buf) {
	std::string *buffer = new std::string(buf);
	size_t start_pos = 0;
	static const std::string LF = "\n";
	static const std::string CRLF = "\r\n";
	while ((start_pos = buffer->find(LF, start_pos)) != std::string::npos) {
		buffer->replace(start_pos, LF.length(), CRLF);
		start_pos += LF.length() + 1; // +1 to avoid the new \n we just created :P
	}
	return buffer;
}
