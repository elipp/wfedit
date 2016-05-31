#include <Windows.h>
#include <gl\GL.h>
#include <string>
#include <vector>

#include "glext_loader.h"
#include "texture.h"
#include "lodepng.h"

static std::string get_file_extension(const std::string &filename) {
	int i = 0;
	size_t fn_len = filename.length();
	while (i < fn_len && filename[i] != '.') {
		++i;
	}
	std::string ext = filename.substr(i + 1, filename.length() - (i + 1));
	//PRINT("get_file_extension: \"%s\"\n", ext.c_str());
	return ext;
}


static int loadPNG(const std::string &filename, std::vector<unsigned char> &pixels, unified_header_data *out_header, LodePNGColorType colortype) {
	unsigned e = lodepng::decode(pixels, out_header->width, out_header->height, filename, colortype);
	if (colortype == LCT_RGBA) { out_header->bpp = 32; }
	else if (colortype == LCT_GREY) { out_header->bpp = 8; }
	else {
		out_header->bpp = 8;
	}

	// lodePNG loads in a top-to-bottom order, so flip

	const size_t stride = (out_header->bpp / 8) * out_header->width;
	const size_t width = out_header->width;
	unsigned char *temprow = new unsigned char[stride];

	for (int i = 0; i < width/2; ++i) {
		memcpy(temprow, &pixels[i*stride], stride);
		memcpy(&pixels[i*stride], &pixels[((width-1) - i) * stride], stride);
		memcpy(&pixels[((width-1) - i)*stride], temprow, stride);
	}

	delete[] temprow;

	return (e == 0);
}

static int load_pixels(const std::string& filename, std::vector<unsigned char> &pixels, unified_header_data *img_info, LodePNGColorType colortype = LCT_RGBA) {
	unsigned width = 0, height = 0;

	memset(img_info, 0x0, sizeof(*img_info));

	std::string ext = get_file_extension(filename);
	if (ext == "png") {
		if (!loadPNG(filename, pixels, img_info, colortype)) {
			printf("load_pixels: fatal error: loading file %s failed.\n", filename.c_str());
			return 0;
		}
		else {
			return 1;
		}
	}
	else {
		printf("load_pixels: fatal error: unsupported image file extension \"%s\" (only .png is supported)\n", ext.c_str());
		return 0;
	}
}


Texture::Texture(const std::string &filename, const GLint filter_param) : name(filename) {

	//haidi haida. oikea kurahaara ja jakorasia.

	unified_header_data img_info;

	std::vector<unsigned char> pixels;
	if (!load_pixels(filename, pixels, &img_info)) {
		printf("Loading a texture resource failed.\n");
		_otherbad = true;
		return;
	}

	_badheader = _nosuch = _otherbad = false;


	if (IS_POWER_OF_TWO(img_info.width) == 0 && img_info.width == img_info.height) {
		// image is valid, carry on
		GLint internal_format = GL_RGBA;
		GLint input_pixel_format = img_info.bpp == 32 ? GL_RGBA : GL_RGB;
		glEnable(GL_TEXTURE_2D);
		glGenTextures(1, &textureId);
		glBindTexture(GL_TEXTURE_2D, textureId);

		glTexImage2D(GL_TEXTURE_2D, 0, internal_format, img_info.width, img_info.height, 0, input_pixel_format, GL_UNSIGNED_BYTE, (const GLvoid*)&pixels[0]);
		//glTexStorage2D(GL_TEXTURE_2D, 4, internalfmt, width, height); // this is superior to glTexImage2D. only available in GL4 though
		//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, (const GLvoid*)&buffer[0]);

		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_param);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	}

	else {	// if not power of two
		_otherbad = true;
	}

}

