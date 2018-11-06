#pragma once

#include <stdbool.h>
#include "var.h"
#include "render.h"
#include "list.h"

struct object {
	void *user_data;
	struct list_head siblings;
	void (*render)(struct object *);
	var *x, *y, *w, *h;
	struct fb fb;
	bool need_render;

	int nparams;
	var *param[0];
};

struct scene {
	struct object *focus;
	int nlayers;
	struct list_head layer[0];
};
