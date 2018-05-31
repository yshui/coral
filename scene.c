#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "user.h"
#include "object.h"
#include "scene.h"
struct mouse_handler {
	void (*mouse_over)(struct object *, bool is_over);
	void (*mouse_button)(struct object *, uint16_t state, int16_t button);
};
static const struct mouse_handler circle_hdlr;
struct scene_config {
	uint32_t w, h;
};
static void move_rect_0_cb(keyed *v, struct key_frame *k, bool finished, void *ud);
static void move_rect_center_cb(keyed *v, struct key_frame *k, bool finished, void *ud) {
	struct scene_config *scfg = ud;
	keyed_new_quadratic_key(v, 0, 3, move_rect_0_cb, scfg);
}
static void move_rect_0_cb(keyed *v, struct key_frame *k, bool finished, void *ud) {
	struct scene_config *scfg = ud;
	fprintf(stderr, "add new keyframe\n");
	keyed_new_quadratic_key(v, scfg->h/2, 3, move_rect_center_cb, scfg);
}

struct scene *new_scene(int nlayers) {
	struct scene *ret = calloc(1, sizeof(struct scene)+sizeof(struct list_head)*nlayers);
	ret->nlayers = nlayers;
	for (int i = 0; i < nlayers; i++)
		INIT_LIST_HEAD(&ret->layer[i]);
	return ret;
}
struct scene *build_scene(struct interpolate_man *im, struct user *user, size_t nusers, uint32_t w, uint32_t h) {
	for (int i = 0; i < nusers; i++)
		fprintf(stderr, "user: %s\n", user[i].name);
	auto scfg = tmalloc(struct scene_config, 1);
	scfg->w = w;
	scfg->h = h;
	auto x = new_keyed(im, h/2);
	keyed_new_quadratic_key((void *)x, -80, 3, move_rect_0_cb, scfg);
	auto wv = vC(20);
	auto full = vC(255);
	auto zero = vC(0);
	auto rect = new_rect(x, vC(w/2), wv, wv, full, zero, zero, full);

	auto y2 = vADD(x, vC(40));
	auto rect2 = new_rect(vC(h/2), y2, wv, wv, zero, full, zero, full);

	auto x3 = vADD(vNEG(x), vC(h));
	auto rect3 = new_rect(x3, vC(w/2), wv, wv, zero, zero, full, full);

	auto y4 = vADD(vNEG(y2), vC(w));
	auto rect4 = new_rect(vC(h/2), y4, wv, wv, full, full, zero, full);

	auto circle = new_circle(vC(h/2-125), vC(w/2-125), vC(250), vC(250), vC(255), vC(255), vC(255), vC(255), vC(1));
	circle->user_data = (void *)&circle_hdlr;

	auto s = new_scene(1);
	list_add(&rect->siblings, &s->layer[0]);
	list_add(&rect2->siblings, &s->layer[0]);
	list_add(&rect3->siblings, &s->layer[0]);
	list_add(&rect4->siblings, &s->layer[0]);
	list_add(&circle->siblings, &s->layer[0]);
	return s;
}

void circle_over(struct object *obj, bool over) {
	fprintf(stderr, "circle over %d\n", over);
	free(obj->param[0]);
	if (over)
		obj->param[0] = vC(0);
	else
		obj->param[0] = vC(255);
	obj->need_render = true;
}

void circle_button(struct object *obj, uint16_t state, int16_t button) {

}

static const struct mouse_handler circle_hdlr = {
	.mouse_over = circle_over,
	.mouse_button = circle_button,
};

struct object *get_object_at(struct scene *s, uint32_t x, uint32_t y) {
	for (int i = s->nlayers-1; i >= 0; i--) {
		struct object *o;
		list_for_each_entry_reverse(o, &s->layer[i], siblings) {
			if (V(o->x) <= x && V(o->y) <= y &&
			    V(o->x)+V(o->h) > x && V(o->y)+V(o->w) > y)
				return o;
		}
	}
	return NULL;
}

void handle_mouse_move(struct scene *s, uint32_t x, uint32_t y) {
	auto obj = get_object_at(s, x, y);
	fprintf(stderr, "%u %u %p\n", x, y, obj);
	if (obj != s->focus) {
		struct mouse_handler *mh;
		if (s->focus) {
			mh = s->focus->user_data;
			if (mh && mh->mouse_over)
				mh->mouse_over(s->focus, false);
		}
		if (obj) {
			mh = (void *)obj->user_data;
			if (mh && mh->mouse_over)
				mh->mouse_over(obj, true);
		}
		s->focus = obj;
	}
}
