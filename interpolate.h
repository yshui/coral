#pragma once
#include <stdbool.h>
#include "list.h"
struct var;
typedef struct var var;
struct key_frame;
typedef void (*key_cb)(var *v, struct key_frame *k, bool finished, void *ud);
struct key_frame {
	double eta;
	double setpoint;
	key_cb callback;
	void (*advance)(struct var *i, struct key_frame *k, double dt);
	void *ud;
	struct key_frame *next;
};
struct var {
	struct list_head siblings;
	struct key_frame *keys, **last_key;
	double current;
	double last_setpoint;
	double elapsed;
	bool changed;
};
struct interpolate_man;

void interpolate_man_advance(struct interpolate_man *, double);
struct interpolate_man *interpolate_man_new(void);
void interpolate_man_register(struct interpolate_man *, var *);
var *new_var(struct interpolate_man *im, double val);
void var_new_linear_key(var *v, double set, double etc, key_cb cb, void *ud);

#define V(x) ((x)->current)
