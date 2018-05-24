#include <stdlib.h>
#include <stdio.h>
#include "render.h"
#include "interpolate.h"
#include "user.h"
#include "scene.h"
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

	auto s = new_scene(1);
	add_object_to_layer(rect, get_layer(s, 0));
	add_object_to_layer(rect2, get_layer(s, 0));
	add_object_to_layer(rect3, get_layer(s, 0));
	add_object_to_layer(rect4, get_layer(s, 0));
	return s;
}
