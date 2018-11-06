#pragma once
#include <stdbool.h>
#include "list.h"

struct var;
struct key_frame *k;
struct var_ops {
	/// What is the current value of the var.
	double (*val)(struct var *i);

	/// (If the var is time dependent), has the value of the
	/// var changed since the last time we called `val`.
	/// False positive is allowed. False negative is not.
	/// Optional
	bool (*changed)(struct var *i);
};

struct var {
	const struct var_ops *ops;
};

enum op {
	ADD,
	SUB,
	MUL,
	DIV,
	NEG
};

typedef struct var var;
struct keyed_var;
typedef struct keyed_var keyed;

struct time_var {
	struct var base;
	double current;
};

typedef void (*key_cb)(keyed *v, struct key_frame *k, bool finished, void *ud);

var *new_keyed(struct time_var *, double val);
var *new_const(double val);
var *new_arith(enum op op, var *lhs, var *rhs);
void keyed_new_linear_key(keyed *v, double set, double etc, key_cb cb, void *ud);
void keyed_new_quadratic_key(keyed *v, double set, double etc, key_cb cb, void *ud);

#define vADD(a, b) new_arith(ADD, a, b)
#define vNEG(a) new_arith(NEG, a, NULL)
#define vC(a) new_const(a)

extern const struct var_ops keyed_var_ops;
extern const struct var_ops const_var_ops;
extern const struct var_ops arith_var_ops;
extern const struct var_ops time_var_ops;

static inline double var_val(var *x) {
	return x->ops->val(x);
}

static inline bool var_changed(var *x) {
	return (x->ops->changed ? x->ops->changed(x) : false);
}

static inline struct time_var new_time(double now) {
	return (struct time_var) {
		.base = { .ops = &time_var_ops },
		.current = now,
	};
}

#define VK(x) (keyed_var_ops.current(x))
#define CK(x) (keyed_var_ops.changed(x))
