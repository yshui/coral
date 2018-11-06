#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <limits.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb/stb_image_resize.h>
#include "common.h"
#include "list.h"
#include "interpolate.h"
#include "object.h"

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

static inline void
pixel(struct fb *fb, uint32_t x, uint32_t y, struct color c) {
	uint32_t off = x*fb->pitch+y*pixfmt_bpp(fb->pixfmt);
	fb->data[off] = c.b*c.a/255.0;
	fb->data[off+1] = c.g*c.a/255.0;
	fb->data[off+2] = c.r*c.a/255.0;
	fb->data[off+3] = c.a;
}

static inline void
xline(struct fb *fb, uint32_t x1, uint32_t x2, uint32_t y,
      struct color c) {
	while (x1 <= x2) pixel(fb, x1++, y, c);
}

static inline void
yline(struct fb *fb, uint32_t x, uint32_t y1, uint32_t y2,
      struct color c) {
	while (y1 <= y2) pixel(fb, x, y1++, c);
}


void render_object(struct object *obj) {
	bool need_rerender = obj->need_render;
	if (!obj->render) {
		assert(obj->fb.data == NULL);
		return;
	}
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
	if (need_rerender) {
		if (!obj->fb.data) {
			obj->fb.width = V(obj->w);
			obj->fb.pitch = obj->fb.width*pixfmt_bpp(obj->fb.pixfmt);
			obj->fb.height = V(obj->h);
			obj->fb.data = calloc(obj->fb.height*obj->fb.pitch, 1);
		}
		obj->render(obj);
		obj->need_render = false;
	}
}

void render_scene(struct fb *fb, struct scene *s) {
	for (int i = 0; i < s->nlayers; i++) {
		struct object *o;
		list_for_each_entry(o, &s->layer[i], siblings) {
			render_object(o);
			if (o->fb.data)
				blit(fb, &o->fb, V(o->x), V(o->y));
		}
	}
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

struct circle {
	struct object base;
	var *r, *g, *b, *a;
	var *thickness;
};

static void render_circle(struct object *o) {
	struct circle *ci = (void *)o;
	struct fb *fb = &o->fb;
	int th = V(ci->thickness);
	int ro = fminf(V(o->w), V(o->h))/2.0-1;
	int xo = ro;
	int xi = ro-th;
	int y = 0;
	int erro = 1 - xo;
	int erri = 1 - xi;

	struct color c = {
		.r = color_clamp(V(ci->r)),
		.g = color_clamp(V(ci->g)),
		.b = color_clamp(V(ci->b)),
		.a = color_clamp(V(ci->a))
	};

	while(xo >= y) {
		xline(fb, ro+xi, ro+xo, ro+y,  c);
		yline(fb, ro+y,  ro+xi, ro+xo, c);
		xline(fb, ro-xo, ro-xi, ro+y,  c);
		yline(fb, ro-y,  ro+xi, ro+xo, c);
		xline(fb, ro-xo, ro-xi, ro-y,  c);
		yline(fb, ro-y,  ro-xo, ro-xi, c);
		xline(fb, ro+xi, ro+xo, ro-y,  c);
		yline(fb, ro+y,  ro-xo, ro-xi, c);

		y++;

		if (erro < 0) {
			erro += 2*y+1;
		} else {
			xo--;
			erro += 2*(y-xo+1);
		}

		if (y > ro-th) {
			xi = y;
		} else {
			if (erri < 0) {
				erri += 2*y+1;
			} else {
				xi--;
				erri += 2*(y-xi+1);
			}
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

struct object *new_obj(var *x, var *y, var *w, var *h, int nparams) {
	struct object *ret = calloc(1, sizeof(struct object)+sizeof(var*)*nparams);
	ret->x = x;
	ret->y = y;
	ret->w = w;
	ret->h = h;
	ret->fb.pixfmt = XRGB8888;
	ret->need_render = true;

	ret->nparams = nparams;
	return ret;
}

struct object *new_ghost(var *x, var *y, var *w, var *h, void *ud) {
	auto ret = new_obj(x, y, w, h, 0);
	ret->user_data = ud;
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

struct object *new_scale(var *x, var *y, var *w, var *h, struct fb *src) {
	struct scale *s = (void *)new_obj(x, y, w, h, 0);
	s->fb = src;
	s->base.render = render_scale;
	return &s->base;
}

struct object *new_circle(var *x, var *y, var *w, var *h, var *r, var *g, var *b, var *a, var *th) {
	struct circle *c = (void *)new_obj(x, y, w, h, 5);
	c->r = r;
	c->g = g;
	c->b = b;
	c->a = a;
	c->thickness = th;
	c->base.render = render_circle;
	return &c->base;
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

void add_object_to_layer(struct object *o, struct list_head *l) {
	list_add(&o->siblings, l);
}
