#include "glwindow.h"

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>
#include <random>
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

static GLuint wave_VBOid, wave_VAOid;

bool fullscreen = false;
bool active = TRUE;

static Texture *gradient_texture;
static ShaderProgram *wave_shader, *point_shader;

static bool _main_loop_running = true;
bool main_loop_running() { return _main_loop_running; }
void stop_main_loop() { _main_loop_running = false; }

#define NUM_CURVES 64

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

vec4 solve_equation_coefs(const float *points) {

	const float& a = points[0];
	const float& b = points[2];
	const float& c = points[4];
	const float& d = points[6];

	mat4 m = mat4(vec4(a*a*a, a*a, a, 1), vec4(b*b*b, b*b, b, 1), vec4(c*c*c, c*c, c, 1), vec4(d*d*d, d*d, d, 1));
	m.invert();

	vec4 correct = m.transposed() * vec4(points[1], points[3], points[5], points[7]);

	return correct;

	//
	////printf("(%.2f, %.2f, %.2f, %.2f)\n(%.2f, %.2f, %.2f, %.2f)\n(%.2f, %.2f, %.2f, %.2f)\n(%.2f, %.2f, %.2f, %.2f)\n",
	////	m(0, 0), m(1, 0), m(2, 0), m(3, 0),
	////	m(0, 1), m(1, 1), m(2, 1), m(3, 1),
	////	m(0, 2), m(1, 2), m(2, 2), m(3, 2),
	////	m(0, 3), m(1, 3), m(2, 3), m(3, 3));

	////printf("correct coefs: (%.2f, %.2f, %.2f, %.2f)\n\n", correct(0), correct(1), correct(2), correct(3));

	//static const vec4 minus_one(-1, -1, -1, -1);

	//vec4 abc1(a, b, c, 1);
	//vec4 abcm1 = abc1 + minus_one;

	//float apb = a + b;
	//float apc = a + c;
	//float bpc = b + c;

	//float amb = a - b;
	//float amc = a - c;
	//float bmc = b - c;
	//float cmb = c - b;

	//float ab = a*b;
	//float ac = a*c;
	//float bc = b*c;

	//vec4 c1_numer(1, -(bpc + 1), bc + bpc, -bc);
	//vec4 c2_numer(-1, apc + 1, -(ac + apc), ac);
	//vec4 c3_numer(1, -(apb + 1), ab + apb, -ab);
	//vec4 c4_numer(-1, apb + c, -(bc + a*bpc), ab*c);
	//
	//vec4 c1_denom(1.0/(abcm1(0) * amb * amc));
	//vec4 c2_denom(1.0/(amb * abcm1(1) * bmc));
	//vec4 c3_denom(1.0/(amc * bmc * abcm1(2)));
	//vec4 c4_denom(1.0/(abcm1(0) * abcm1(1) *abcm1(2)));
	//
	//mat4 mi = mat4(componentwise_multiply(c1_numer, c1_denom),
	//	componentwise_multiply(c2_numer, c2_denom),
	//	componentwise_multiply(c3_numer, c3_denom),
	//	componentwise_multiply(c4_numer, c4_denom));

	//vec4 other = mi*vec4(points[1], points[3], points[5], points[7]);

	////printf("(%.2f, %.2f, %.2f, %.2f)\n(%.2f, %.2f, %.2f, %.2f)\n(%.2f, %.2f, %.2f, %.2f)\n(%.2f, %.2f, %.2f, %.2f)\n",
	////	mi(0, 0), mi(1, 0), mi(2, 0), mi(3, 0),
	////	mi(0, 1), mi(1, 1), mi(2, 1), mi(3, 1),
	////	mi(0, 2), mi(1, 2), mi(2, 2), mi(3, 2),
	////	mi(0, 3), mi(1, 3), mi(2, 3), mi(3, 3));
	////printf("calculated coefs: (%.2f, %.2f, %.2f, %.2f)\n\n", other(0), other(1), other(2), other(3));

	//return other;
}

static float GT = 0;


void update_data() {

	GT += 0.001;

	static float patch_buffer[NUM_CURVES];
	//static std::mt19937 rng(time(0));
	//static vec4 eq_coefs[100];
	//
	//static auto rand_float = std::bind(std::uniform_real_distribution<float>(-1, 1), rng);

	for (int i = 0; i < NUM_CURVES; ++i) {
		patch_buffer[i] = (float)i / (float)NUM_CURVES;
	}


	mat4 m = mat4(vec4(0, 0, 0, 1), vec4((0.33*0.33*0.33), (0.33*0.33), 0.33, 1), vec4((0.66*0.66*0.66), (0.66*0.66), 0.66, 1), vec4(1, 1, 1, 1));
	m.invert();

	glUseProgram(wave_shader->getProgramHandle());
	wave_shader->update_uniform_mat4("coefs_inv", m);

	//printf("a = %f, b = %f, c = %f, d = %f\n", eq_coefs(0), eq_coefs(1), eq_coefs(2), eq_coefs(3));

	glBindBuffer(GL_ARRAY_BUFFER, wave_VBOid);
	glBufferSubData(GL_ARRAY_BUFFER, 0, NUM_CURVES*sizeof(float), patch_buffer);

}


void draw() {

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(wave_shader->getProgramHandle());

//	mat4 mvp = mat4::proj_ortho(0.0, WIN_W, WIN_H, 0.0, -1.0, 1.0) * mat4::translate(0.0, (WIN_H / 2), 0.0) * mat4::scale(WIN_W, WIN_H, 1.0);
	mat4 mvp = mat4::proj_ortho(0.0, 1.0, -1.0, 1.0, -1.0, 1.0);
//	mat4 mvp = mat4::proj_ortho(0.0, 50, -50, 50, -1.0, 1.0);

	wave_shader->update_uniform_mat4("uMVP", mvp);
	
	GT += 0.006;
	
	wave_shader->update_uniform_1f("TIME", GT);

	glBindVertexArray(wave_VAOid);	
	glDrawArrays(GL_PATCHES, 0, NUM_CURVES);
	
	//glUseProgram(point_shader->getProgramHandle());

	//point_shader->update_uniform_mat4("uMVP", mvp);

	//glDrawArrays(GL_POINTS, 0, 4);

}

int init_GL() {

	if (!load_GL_extensions()) {
		return 0;
	}

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glEnable(GL_DEPTH_TEST);

	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//glEnable(GL_BLEND);

	wglSwapIntervalEXT(1);

	glViewport(0, 0, WIN_W, WIN_H);

	const char* version = (const char*)glGetString(GL_VERSION);

	printf("OpenGL version information:\n%s\n\n", version);

	GLint max_elements_vertices, max_elements_indices;
	glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &max_elements_vertices);
	glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &max_elements_indices);

	glPatchParameteri(GL_PATCH_VERTICES, 1);

	printf("GL_MAX_ELEMENTS_VERTICES = %d\nGL_MAX_ELEMENTS_INDICES = %d\n", max_elements_vertices, max_elements_indices);


#define ADD_ATTRIB(map, attrib_id, attrib_name) do {\
	(map).insert(std::make_pair<GLuint, std::string>((attrib_id), std::string(attrib_name)));\
	} while(0)

	std::unordered_map<GLuint, std::string> default_attrib_bindings;
	ADD_ATTRIB(default_attrib_bindings, ATTRIB_POSITION, "Position_VS_in");

	wave_shader = new ShaderProgram("shaders/wave", default_attrib_bindings);
	point_shader = new ShaderProgram("shaders/pointplot", default_attrib_bindings);

	glGenVertexArrays(1, &wave_VAOid);
	glBindVertexArray(wave_VAOid);

	glEnableVertexAttribArray(ATTRIB_POSITION);

	glGenBuffers(1, &wave_VBOid);
	glBindBuffer(GL_ARRAY_BUFFER, wave_VBOid);
	glBufferData(GL_ARRAY_BUFFER, NUM_CURVES*sizeof(float), NULL, GL_DYNAMIC_DRAW);

	glVertexAttribPointer(ATTRIB_POSITION, 1, GL_FLOAT, GL_FALSE, 1*sizeof(float), 0);

	glBindVertexArray(0);

	//glDisableVertexAttribArray(ATTRIB_POSITION);

	update_data();

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
