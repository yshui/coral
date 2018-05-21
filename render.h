#pragma once
#include <stdint.h>
#include <stdbool.h>

struct var;
typedef struct var var;
// Pixel format is assumed to be XRGB/ARGB8888 (high bytes to low bytes)

enum pixfmt {
	XRGB8888,
	ARGB8888,
};

struct fb {
	uint8_t * restrict data;
	uint32_t pitch;
	int32_t height;
	int32_t width;

	enum pixfmt pixfmt;
};

struct scene;
struct object;
struct layer;
void render_scene(struct fb *, struct scene *, bool force);
struct object *new_rect(var *x, var *y, var *w, var *h, var *r, var *g, var *b, var *a);
void add_object_to_layer(struct object *o, struct layer *l);
void set_object_parent(struct object *, struct object *parent);
struct layer *get_layer(struct scene *s, int n);
struct scene *new_scene(int nlayers);
struct fb* new_similar_fb(const struct fb *old);

static inline uint8_t pixfmt_bpp(uint8_t pixfmt) {
	return 4;
}
static inline bool pixfmt_compat(uint8_t p1, uint8_t p2) {
	return true;
}
