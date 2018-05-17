#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "list.h"
#include "interpolate.h"
#include "common.h"

struct interpolate_man {
	struct list_head interpolatables;
};

void interpolatable_linear(var *i, struct key_frame *k, double dt) {
	double d = (k->setpoint-i->last_setpoint)/(k->eta+i->elapsed)*dt;
	i->changed = true;
	i->current += d;
}

// XXX this won't work with current model
void interpolatable_step(var *i, struct key_frame *k, double dt) {
	int pos = (k->setpoint-i->last_setpoint)/(k->eta+i->elapsed)*(k->eta+dt);
	double old = i->current;
	i->current = pos;
	i->changed = fabs(old-i->current) > 1e-6;
}

void interpolatable_append_keyframe(var *i, struct key_frame *k) {
	*i->last_key = k;
	i->last_key = &k->next;
}

void interpolatable_truncate(var *i) {
	auto k = i->keys;
	while(k) {
		auto next = k->next;
		free(k);
		k = next;
	}
	i->elapsed = 0;
	i->last_setpoint = i->current;
}

void interpolatable_advance(var *i, double dt) {
	struct key_frame *new_head = i->keys;

	// Remove expired frames, set changed to true, update last_setpoint,
	// reset elapsed
	double changed = false;
	while(dt >= new_head->eta) {
		struct key_frame *next = new_head->next;
		if (new_head->callback)
			new_head->callback(i, new_head, true, next->ud);
		dt -= new_head->eta;
		i->last_setpoint = new_head->setpoint;
		i->current = new_head->setpoint;
		i->elapsed = 0;
		changed = true;
		free(new_head);
		new_head = next;
	}

	// Call advance on current key frame
	i->elapsed += dt;
	if (new_head->advance)
		new_head->advance(i, new_head, dt);
	i->changed = i->changed || changed;
	new_head->eta -= dt;
	i->keys = new_head;
}

void interpolate_man_advance(struct interpolate_man *im, double dt) {
	var *i;
	list_for_each_entry(i, &im->interpolatables, siblings) {
		if (!i->keys) {
			i->changed = false;
			continue;
		} else
			interpolatable_advance(i, dt);
	}
}

void interpolate_man_register(struct interpolate_man *im, var *i) {
	list_add(&i->siblings, &im->interpolatables);
}

struct interpolate_man *interpolate_man_new(void) {
	auto im = tmalloc(struct interpolate_man, 1);
	INIT_LIST_HEAD(&im->interpolatables);
	return im;
}

void var_new_linear_key(var *v, double setpoint, double eta, key_cb cb, void *ud) {
	auto k = tmalloc(struct key_frame, 1);
	k->eta = eta;
	k->setpoint = setpoint;
	k->callback = cb;
	k->advance = interpolatable_linear;
	k->ud = ud;
	*v->last_key = k;
	v->last_key = &k->next;
}

var *new_var(struct interpolate_man *im, double val) {
	var *ret = tmalloc(var, 1);
	ret->current = ret->last_setpoint = val;
	ret->elapsed = 0;
	ret->last_key = &ret->keys;
	if (im)
		interpolate_man_register(im, ret);
	return ret;
}
