#pragma once

#include "glext_loader.h"

#define IS_POWER_OF_TWO(x) ((x) & ((x) - 1))

struct unified_header_data {
	unsigned width, height;
	int bpp;
};

class Texture {
private:
	std::string name;
	GLuint textureId;
	unified_header_data img_info;

	bool _nosuch;
	bool _badheader;
	bool _otherbad;

public:
	std::string getName() const { return name; }
	GLuint id() const { return textureId; }

	bool bad() const { return _nosuch || _badheader || _otherbad; }
	bool nosuch() const { return _nosuch; }
	bool badheader() const { return _badheader; }
	bool otherbad() const { return _otherbad; }

	GLuint getId() const { return textureId; }
	Texture(const std::string &filename, const GLint filter_param);
	Texture() {};

};