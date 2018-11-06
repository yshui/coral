#pragma once
#include <stdint.h>
#include <stdbool.h>

#include "common.h"

struct var;
struct list_head;
typedef struct var var;
// Pixel format is assumed to be XRGB/ARGB8888 (high bytes to low bytes)

enum pixfmt {
	XRGB8888,
	ARGB8888,
};

struct color {
	double r, g, b, a;
};

struct fb {
	uint8_t * restrict data;
	uint32_t pitch;
	int32_t height;
	int32_t width;

	enum pixfmt pixfmt;
};

#define POS_PARAMS var *x, var *y, var *w, var *h

struct scene;
struct object;
void render_scene(struct fb *, struct scene *);
struct object *new_rect(POS_PARAMS, var *r, var *g, var *b, var *a);
struct object *new_circle(POS_PARAMS, var *r, var *g, var *b, var *a, var *thickness);
struct object *new_ghost(POS_PARAMS, void *ud);
struct object *new_scale(POS_PARAMS, struct fb *src);
struct scene *new_scene(int nlayers);
struct fb* new_similar_fb(const struct fb *old);

static inline uint8_t pixfmt_bpp(uint8_t pixfmt) {
	return 4;
}
static inline bool pixfmt_compat(uint8_t p1, uint8_t p2) {
	return true;
}

static inline struct fb* new_fb(int32_t w, int32_t h, enum pixfmt pixfmt) {
	auto ret = tmalloc(struct fb, 1);
	ret->width = w;
	ret->height = h;
	ret->pitch = w*pixfmt_bpp(pixfmt);
	ret->pixfmt = pixfmt;
	ret->data = calloc(1, ret->pitch*h);
	return ret;
}
