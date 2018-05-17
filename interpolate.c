#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "list.h"
#include "interpolate.h"

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
			new_head->callback(new_head, true);
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
