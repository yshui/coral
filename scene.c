#include <stdlib.h>
#include <stdio.h>
#include "render.h"
#include "interpolate.h"
struct scene_config {
	uint32_t w, h;
};
void move_rect_0_cb(var *v, struct key_frame *k, bool finished, void *ud);
void move_rect_center_cb(var *v, struct key_frame *k, bool finished, void *ud) {
	struct scene_config *scfg = ud;
	var_new_linear_key(v, 0, 5, move_rect_0_cb, scfg);
}
void move_rect_0_cb(var *v, struct key_frame *k, bool finished, void *ud) {
	struct scene_config *scfg = ud;
	fprintf(stderr, "add new keyframe\n");
	var_new_linear_key(v, scfg->h/2, 5, move_rect_center_cb, scfg);
}
struct scene *build_scene(struct interpolate_man *im, uint32_t w, uint32_t h) {
	auto scfg = tmalloc(struct scene_config, 1);
	scfg->w = w;
	scfg->h = h;
	var *x = new_var(im, h/2);
	var_new_linear_key(x, 0, 5, move_rect_0_cb, scfg);
	auto rect =
	    new_rect(
	             x,                //x
	             new_var(im, w/2), //y
	             new_var(im, 20),  //w
	             new_var(im, 20),  //h
	             new_var(im, 255), //r
	             new_var(im, 0),   //g
	             new_var(im, 0),   //b
	             new_var(im, 0)    //a
	   );

	auto s = new_scene(1);
	add_object_to_layer(rect, get_layer(s, 0));
	return s;
}
