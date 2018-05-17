#pragma once

// Pixel format is assumed to be XRGB/ARGB8888 (high bytes to low bytes)
struct fb {
	uint8_t *data;
	uint32_t pitch;
	uint32_t height;
	uint32_t width;

	unsigned short bpp;
};
