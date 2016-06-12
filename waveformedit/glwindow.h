#pragma once

#include <Windows.h>
#include <gl/GL.h>
#include "glext_loader.h"

#define WIN_W 1600
#define WIN_H 900

void swap_buffers();

int create_GL_window(const char* title, int width, int height);
int init_GL();

void draw();