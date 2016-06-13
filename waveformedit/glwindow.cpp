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
#include "sound.h"
#include "curve.h"

bool mouse_locked = false;

static HGLRC hRC = NULL;
static HDC hDC = NULL;
static HWND hWnd = NULL;
static HINSTANCE hInstance;

extern HINSTANCE win_hInstance;

static HWND hWnd_child = NULL;

unsigned WINDOW_WIDTH = WIN_W;
unsigned WINDOW_HEIGHT = WIN_H;

static GLuint wave_VBOid, wave_VAOid;

bool fullscreen = false;
bool active = TRUE;

static Texture *gradient_texture;
static ShaderProgram *wave_shader, *point_shader, *grid_shader;

static bool _main_loop_running = true;
bool main_loop_running() { return _main_loop_running; }
void stop_main_loop() { _main_loop_running = false; }



#define NUM_CURVES 1

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
}

static float *sample_buffer = NULL;

static float GT = 0;

static int allocate_static_buf(size_t size) {
	if (sample_buffer == NULL) {
		sample_buffer = new float[size];
		return 1;
	}
	return 0;
}



static int get_samples(const vec4 &coefs, wave_format_t *fmt) {
	UINT32 frame_size = SND_get_frame_size();

	float dt = 1.0 / (float)frame_size;
	float x = 0;

	for (int i = 0; i < frame_size; ++i) {
		float x2 = x*x;
		float x3 = x2*x;
		vec4 tmp = vec4(x3, x2, x, 1);
		
		float v = 0.6*dot4(coefs, tmp);

		//printf("v: %.4f\n", v);
	
		sample_buffer[2*i] = v;
		sample_buffer[2*i + 1] = v;

		x += dt;
	}


	return 1;

}

void update_data() {

	//static float patch_buffer[NUM_CURVES];

	//for (int i = 0; i < NUM_CURVES; ++i) {
	//	patch_buffer[i] = (float)i / (float)NUM_CURVES;
	//}

	mat4 m = mat4(
		vec4(0, 0, 0, 1), 
		vec4((0.33*0.33*0.33), (0.33*0.33), 0.33, 1), 
		vec4((0.66*0.66*0.66), (0.66*0.66), 0.66, 1), 
		vec4(1, 1, 1, 1));
	
	m.invert();

	float y0, y1, y2, y3;
	y0 = 0.0;
	y1 = sin(GT);
	y2 = -sin(GT);
	y3 = 0.0;

	float points[8] = {
		0.0, y0,
		0.33, y1,
		0.66, y2,
		1.0, y3
	};

	while (!SND_initialized()) { Sleep(250); }

	wave_format_t fmt = SND_get_format_info();
	vec4 coefs = solve_equation_coefs(points);
	allocate_static_buf(fmt.num_channels * SND_get_frame_size());
	get_samples(coefs, &fmt);

	SND_write_to_buffer(sample_buffer);

	glUseProgram(wave_shader->getProgramHandle());
	wave_shader->update_uniform_mat4("coefs_inv", m);
	wave_shader->update_uniform_vec4("y_coords", vec4(y0, y1, y2, y3));

	//glBindBuffer(GL_ARRAY_BUFFER, wave_VBOid);
	//glBufferSubData(GL_ARRAY_BUFFER, 0, NUM_CURVES*sizeof(float), patch_buffer);

}


void draw() {

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(wave_shader->getProgramHandle());

	mat4 mvp = mat4::proj_ortho(-0.1, 1.1, -1.5, 1.5, -1.0, 1.0);

	wave_shader->update_uniform_mat4("uMVP", mvp);
	GT += 0.006;
	
	wave_shader->update_uniform_1f("TIME", GT);

	update_data();

	glBindVertexArray(wave_VAOid);	
	glDrawArrays(GL_PATCHES, 0, NUM_CURVES);

	glBindVertexArray(0);

	glUseProgram(grid_shader->getProgramHandle());
	grid_shader->update_uniform_1f("tess_level", 11);
	grid_shader->update_uniform_mat4("uMVP", mvp);
	glDrawArrays(GL_PATCHES, 0, 1);

	/*grid_shader->update_uniform_1f("tess_level", 22);
	mvp = mvp * mat4::scale(0.50, 2.0, 0) * mat4::rotate(3.1415926536 / 2.0, 0, 0, 1) * mat4::translate(-0.5, 1.0, 0.0);
	grid_shader->update_uniform_mat4("uMVP", mvp);
	glDrawArrays(GL_PATCHES, 0, 1);*/

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
	grid_shader = new ShaderProgram("shaders/grid", default_attrib_bindings);

	// TODO: CHECK SHADERS FOR BADNESS :D

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
