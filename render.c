#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "common.h"
#include "list.h"
#include "interpolate.h"
#include "render.h"

struct object {
	struct object *parent;
	struct list_head siblings;
	void (*render)(struct object *, const struct fb *);

	int nparams;
	var *param[0];
};

struct layer {
	struct fb cache;
	struct list_head objs;
};

struct scene {
	int nlayers;
	struct layer layer[0];
};

static inline void blend(struct fb *bottom, struct fb *top) {
	assert(bottom->pitch == top->pitch);
	assert(bottom->height == top->height);
	assert(bottom->width == top->width);
	assert(bottom->bpp == top->bpp);
	assert(top->bpp == 4);
	for (uint32_t i = 0; i < top->pitch*top->height*top->bpp; i += top->bpp) {
		double ialpha = (255.0-top->data[i+3])/255.0;
		bottom->data[i] = bottom->data[i]*ialpha+top->data[i];
		bottom->data[i+1] = bottom->data[i+1]*ialpha+top->data[i+1];
		bottom->data[i+2] = bottom->data[i+2]*ialpha+top->data[i+2];
	}
}

bool is_layer_updated(struct layer *l) {
	struct object *o;
	list_for_each_entry(o, &l->objs, siblings) {
		for (int i = 0; i < o->nparams; i++)
			if (o->param[i]->changed)
				return true;
	}
	return false;
}

void render_layer(struct layer *l) {
	struct object *o;
	list_for_each_entry(o, &l->objs, siblings) {
		assert(o->render);
		o->render(o, &l->cache);
	}
}

void render_scene(struct fb *fb, struct scene *s) {
	for (int i = 0; i < s->nlayers; i++) {
		if (is_layer_updated(s->layer+i)) {
			size_t size = fb->pitch*fb->height;
			memset(s->layer[i].cache.data, 0, size);
			render_layer(s->layer+i);
			blend(fb, &s->layer[i].cache); // should cache blend result too XXX
		}
	}
}

void add_object_to_layer(struct object *o, struct layer *l) {
	list_add(&o->siblings, &l->objs);
}

void set_object_parent(struct object *o, struct object *p) {
	o->parent = p;
}

struct rect {
	struct object base;
	var *x, *y;
	var *width, *height;
	var *r, *g, *b, *a;
};

static inline double color_clamp(double in) {
	if (in < 0) return 0;
	if (in > 255) return 255;
	return in;
}

static void render_rect(struct object *_o, const struct fb *fb) {
	struct rect *o = (void *)_o;
	double r = color_clamp(V(o->r)),
	       g = color_clamp(V(o->g)),
	       b = color_clamp(V(o->b)),
	       a = color_clamp(V(o->a));
	for (uint32_t i = V(o->x); i < V(o->x)+V(o->width); i++) {
		uint32_t ba = i*fb->pitch;
		for (uint32_t j = V(o->y); j < V(o->y)+V(o->height); j++) {
			fb->data[ba+j] = b;
			fb->data[ba+j+1] = g;
			fb->data[ba+j+2] = r;
			fb->data[ba+j+3] = a;
		}
	}
}

struct object *new_rect(var *x, var *y, var *width, var *height, var *r, var *g, var *b, var *a) {
	struct rect *n = tmalloc(struct rect, 1);
	n->x = x;
	n->y = y;
	n->width = width;
	n->height = height;
	n->r = r;
	n->g = g;
	n->b = b;
	n->a = a;
	n->base.nparams = 8;
	n->base.render = render_rect;
	return &n->base;
}
