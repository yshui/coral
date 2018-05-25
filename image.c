#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "render.h"
#include "common.h"

struct fb *load_image(const char *filename) {
	FILE *f = fopen(filename, "r");
	if (!f)
		return NULL;
	int x, y, c;
	auto img = stbi_load_from_callbacks(&stbi__stdio_callbacks, f, &x, &y, &c, 4);
	fclose(f);

	auto a = tmalloc(struct fb, 1);
	a->height = y;
	a->width = x;
	a->pitch = XRGB8888;
	a->pitch = x*4;
	a->data = img;

	return a;
}
