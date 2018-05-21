#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include "list.h"
#include "interpolate.h"
#include "common.h"

struct key_frame;
struct key_frame {
	double eta;
	double setpoint;
	key_cb callback;
	void (*advance)(keyed *i, struct key_frame *k, double dt);
	void *ud;
	struct key_frame *next;
};
struct keyed_var {
	struct var base;
	bool changed;
	double current;
	double last_setpoint;
	double elapsed;
	struct key_frame *keys, **last_key;
};

struct interpolate_man {
	struct list_head interpolatables;
};

void linear_advance(keyed *i, struct key_frame *k, double dt) {
	double d = (k->setpoint-i->last_setpoint)/(k->eta+i->elapsed)*dt;
	i->changed = true;
	i->current += d;
	fprintf(stderr, "curr %lf\n", i->current);
}

// XXX this won't work with current model
void step_advance(keyed *i, struct key_frame *k, double dt) {
	int pos = (k->setpoint-i->last_setpoint)/(k->eta+i->elapsed)*(k->eta+dt);
	double old = i->current;
	i->current = pos;
	i->changed = fabs(old-i->current) > 1e-6;
}

void keyed_append_keyframe(keyed *i, struct key_frame *k) {
	*i->last_key = k;
	i->last_key = &k->next;
}

void keyed_truncate(keyed *i) {
	auto k = i->keys;
	while(k) {
		auto next = k->next;
		free(k);
		k = next;
	}
	i->elapsed = 0;
	i->last_setpoint = i->current;
}

static void keyed_var_advance(var *_i, double dt) {
	keyed *i = (void *)_i;
	struct key_frame *new_head = i->keys;
	if (!new_head) {
		i->changed = false;
		return;
	}

	// Remove expired frames, set changed to true, update last_setpoint,
	// reset elapsed
	double changed = false;
	while(new_head && dt >= new_head->eta) {
		if (new_head->callback)
			new_head->callback(i, new_head, true, new_head->ud);
		dt -= new_head->eta;
		i->last_setpoint = new_head->setpoint;
		i->current = new_head->setpoint;
		i->elapsed = 0;
		changed = true;

		struct key_frame *next = new_head->next;
		free(new_head);
		new_head = next;
	}

	// Call advance on current key frame
	i->elapsed += dt;
	if (new_head) {
		if (new_head->advance)
			new_head->advance(i, new_head, dt);
		new_head->eta -= dt;
	} else
		i->last_key = &i->keys;

	i->changed = i->changed || changed;
	i->keys = new_head;
}

void interpolate_man_advance(struct interpolate_man *im, double dt) {
	var *i;
	list_for_each_entry(i, &im->interpolatables, siblings)
		if (i->ops->advance)
			i->ops->advance(i, dt);
}

void interpolate_man_register(struct interpolate_man *im, var *i) {
	list_add(&i->siblings, &im->interpolatables);
}

struct interpolate_man *interpolate_man_new(void) {
	auto im = tmalloc(struct interpolate_man, 1);
	INIT_LIST_HEAD(&im->interpolatables);
	return im;
}

void keyed_new_linear_key(keyed *v, double setpoint, double eta, key_cb cb, void *ud) {
	auto k = tmalloc(struct key_frame, 1);
	k->eta = eta;
	k->setpoint = setpoint;
	k->callback = cb;
	k->advance = linear_advance;
	k->ud = ud;
	keyed_append_keyframe(v, k);
}

var *new_keyed(struct interpolate_man *im, double val) {
	keyed *ret = tmalloc(struct keyed_var, 1);
	ret->base.ops = &keyed_var_ops;
	ret->changed = true;
	ret->current = ret->last_setpoint = val;
	ret->elapsed = 0;
	ret->last_key = &ret->keys;
	if (im)
		interpolate_man_register(im, &ret->base);
	return &ret->base;
}

struct const_var {
	var base;
	double current;
};

var *new_const(double val) {
	auto ret = tmalloc(struct const_var, 1);
	ret->base.ops = &const_var_ops;
	ret->current = val;
	return &ret->base;
}

struct arith_var {
	var base;
	var *a, *b;
	enum op op;
};

var *new_arith(enum op op, var *a, var *b) {
	auto ret = tmalloc(struct arith_var, 1);
	ret->base.ops = &arith_var_ops;
	ret->a = a;
	ret->b = b;
	ret->op = op;
	return &ret->base;
}

static double const_var_current(var *i) {
	return ((struct const_var *)i)->current;
}

static bool keyed_var_changed(var *i) {
	return ((struct keyed_var *)i)->changed;
}

static double keyed_var_current(var *i) {
	return ((struct keyed_var *)i)->current;
}

static bool arith_var_changed(var *i) {
	struct arith_var *v = (void *)i;
	return C(v->a) || C(v->b);
}

static double arith_var_current(var *i) {
	struct arith_var *v = (void *)i;
	switch(v->op) {
	case ADD: return V(v->a)+V(v->b);
	case SUB: return V(v->a)-V(v->b);
	case MUL: return V(v->a)*V(v->b);
	case DIV: return V(v->a)/V(v->b);
	case NEG: return -V(v->a);
	default: assert(false); __builtin_unreachable();
	}
}

const struct var_ops keyed_var_ops = {
	.changed = keyed_var_changed,
	.advance = keyed_var_advance,
	.current = keyed_var_current,
};

const struct var_ops const_var_ops = {
	.current = const_var_current,
};

const struct var_ops arith_var_ops = {
	.changed = arith_var_changed,
	.current = arith_var_current,
};
