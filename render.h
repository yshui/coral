#pragma once
#include <stdint.h>

struct var;
typedef struct var var;
// Pixel format is assumed to be XRGB/ARGB8888 (high bytes to low bytes)
struct fb {
	uint8_t *data;
	uint32_t pitch;
	uint32_t height;
	uint32_t width;

	unsigned short bpp;
};

struct scene;
struct object;
struct layer;
void render_scene(struct fb *, struct scene *);
struct object *new_rect(var *x, var *y, var *w, var *h, var *r, var *g, var *b, var *a);
void add_object_to_layer(struct object *o, struct layer *l);
void set_object_parent(struct object *, struct object *parent);
struct layer *get_layer(struct scene *s, int n);
struct scene *new_scene(int nlayers);
