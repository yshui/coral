#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <limits.h>
#include <stb/stb_image_resize.h>
#include "common.h"
#include "list.h"
#include "interpolate.h"
#include "render.h"

struct object {
	struct list_head siblings;
	void (*render)(struct object *);
	var *x, *y, *w, *h;
	struct fb fb;

	int nparams;
	var *param[0];
};

struct layer {
	struct fb *cache;
	struct list_head objs;
};

struct scene {
	int nlayers;
	struct layer layer[0];
};

void blit(const struct fb *bottom, const struct fb *top,
	  int32_t x, int32_t y) {
	assert(pixfmt_compat(bottom->pixfmt, top->pixfmt));
	assert(top->height <= INT_MAX);
	assert(top->data);

	int32_t w = top->width, h = top->height;
	//Clipping
	if (x >= bottom->height || y >= bottom->width)
		// outside
		return;
	if (x < -h || y < -w)
		// outside
		return;

	int32_t src_x = 0, src_y = 0;
	// clip top
	if (x < 0)
		src_x = -x;
	// clip left
	if (y < 0)
		src_y = -y;
	// clip bottom
	if (x+h >= bottom->height)
		h = bottom->height-x-1;
	//clip right
	if (y+w >= bottom->width)
		w = bottom->width-y-1;

	if (top->pixfmt == XRGB8888) {
		uint32_t real_pitch = (w-src_y)*pixfmt_bpp(top->pixfmt);
		for (uint32_t i = src_x; i < h; i++) {
			auto basea = i*top->pitch+src_y*pixfmt_bpp(top->pixfmt);
			auto baseb = (i+x)*bottom->pitch+(y+src_y)*pixfmt_bpp(bottom->pixfmt);
			memcpy(bottom->data+baseb, top->data+basea, real_pitch);
		}
		return;
	}

	for (uint32_t i = src_x; i < h; i++) {
		auto basea = i*top->pitch+src_y*pixfmt_bpp(top->pixfmt);
		auto baseb = (i+x)*bottom->pitch+y*pixfmt_bpp(bottom->pixfmt);
		for (uint32_t j = src_y; j < w; j++) {
			if (!top->data[basea+3])
				continue;

			double ialpha = (255.0-top->data[basea+3])/255.0;
			bottom->data[baseb] = bottom->data[baseb]*ialpha+top->data[basea];
			bottom->data[baseb+1] = bottom->data[baseb+1]*ialpha+top->data[basea+1];
			bottom->data[baseb+2] = bottom->data[baseb+2]*ialpha+top->data[basea+2];
			basea += pixfmt_bpp(bottom->pixfmt);
			baseb += pixfmt_bpp(top->pixfmt);
		}
	}
}

void render_object(struct object *obj, bool force) {
	bool need_rerender = false;
	if (C(obj->w) || C(obj->h)) {
		need_rerender = true;
		free(obj->fb.data);
		obj->fb.data = NULL;
	}
	for (int i = 0; i < obj->nparams; i++)
		if (C(obj->param[i])) {
			need_rerender = true;
			break;
		}
	if (force || need_rerender) {
		if (!obj->fb.data) {
			obj->fb.width = V(obj->w);
			obj->fb.pitch = obj->fb.width*pixfmt_bpp(obj->fb.pixfmt);
			obj->fb.height = V(obj->h);
			obj->fb.data = malloc(obj->fb.height*obj->fb.pitch);
		}
		obj->render(obj);
	}
}

void render_scene(struct fb *fb, struct scene *s, bool force) {
	for (int i = 0; i < s->nlayers; i++) {
		struct object *o;
		list_for_each_entry(o, &s->layer[i].objs, siblings) {
			render_object(o, force);
			blit(fb, &o->fb, V(o->x), V(o->y));
		}
	}
}

void add_object_to_layer(struct object *o, struct layer *l) {
	list_add(&o->siblings, &l->objs);
}

struct rect {
	struct object base;
	var *r, *g, *b, *a;
};

static inline double color_clamp(double in) {
	if (in < 0) return 0;
	if (in > 255) return 255;
	return in;
}

static void render_rect(struct object *_o) {
	struct rect *o = (void *)_o;
	double r = color_clamp(V(o->r)),
	       g = color_clamp(V(o->g)),
	       b = color_clamp(V(o->b)),
	       a = color_clamp(V(o->a));

	uint8_t *data = o->base.fb.data;

	for (uint32_t i = 0; i < V(o->base.h); i++) {
		uint32_t ba = i*_o->fb.pitch;
		for (uint32_t j = 0; j < V(o->base.w); j++) {
			data[ba+j*4] = b*a/255.0;
			data[ba+j*4+1] = g*a/255.0;
			data[ba+j*4+2] = r*a/255.0;
			data[ba+j*4+3] = a;
		}
	}
}

struct scale {
	struct object base;
	struct fb *fb;
};

static void render_scale(struct object *_o) {
	struct scale *o = (void *)_o;
	stbir_resize_uint8_generic(o->fb->data, o->fb->width, o->fb->height, 0,
	                           o->base.fb.data, o->base.fb.width, o->base.fb.height, 0,
	                           4, 0, STBIR_FLAG_ALPHA_PREMULTIPLIED, STBIR_EDGE_CLAMP,
	                           STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, NULL);
}

void set_obj_pixfmt(struct object *o, uint8_t pixfmt) {
	o->fb.pixfmt = pixfmt;
}

struct object *new_obj(var *x, var *y, var *w, var *h, int nparams) {
	struct object *ret = calloc(1, sizeof(struct object)+sizeof(var*)*nparams);
	ret->x = x;
	ret->y = y;
	ret->w = w;
	ret->h = h;
	ret->fb.pixfmt = XRGB8888;

	ret->nparams = nparams;
	return ret;
}

struct object *new_rect(var *x, var *y, var *w, var *h, var *r, var *g, var *b, var *a) {
	struct rect *n = (void *)new_obj(x, y, w, h, 4);
	n->r = r;
	n->g = g;
	n->b = b;
	n->a = a;
	n->base.render = render_rect;
	return &n->base;
}

struct scene *new_scene(int nlayers) {
	struct scene *ret = calloc(1, sizeof(struct scene)+sizeof(struct layer)*nlayers);
	ret->nlayers = nlayers;
	for (int i = 0; i < nlayers; i++)
		INIT_LIST_HEAD(&ret->layer[i].objs);
	return ret;
}

struct layer *get_layer(struct scene *s, int n) {
	 return &s->layer[n];
}

var **get_object_params(struct object *obj, int *nparams) {
	*nparams = obj->nparams;
	return obj->param;
}

struct fb *new_similar_fb(const struct fb *old) {
	auto ret = tmalloc(struct fb, 1);
	*ret = *old;
	ret->data = calloc(ret->pitch, ret->height);
	return ret;
}
