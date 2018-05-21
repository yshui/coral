#pragma once
#include <stdbool.h>
#include "list.h"

struct var;
struct key_frame *k;
struct var_ops {
	double (*current)(struct var *i);
	bool (*changed)(struct var *i);
	void (*advance)(struct var *i, double dt);
};

struct var {
	struct list_head siblings;
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
struct interpolate_man;
typedef void (*key_cb)(keyed *v, struct key_frame *k, bool finished, void *ud);

void interpolate_man_advance(struct interpolate_man *, double);
struct interpolate_man *interpolate_man_new(void);
void interpolate_man_register(struct interpolate_man *, var *);
var *new_keyed(struct interpolate_man *im, double val);
var *new_const(double val);
var *new_arith(enum op op, var *lhs, var *rhs);
void keyed_new_linear_key(keyed *v, double set, double etc, key_cb cb, void *ud);

#define vADD(a, b) new_arith(ADD, a, b)
#define vNEG(a) new_arith(NEG, a, NULL)
#define vC(a) new_const(a)

extern const struct var_ops keyed_var_ops;
extern const struct var_ops const_var_ops;
extern const struct var_ops arith_var_ops;

#define V(x) ((x)->ops->current(x))
#define C(x) ((x)->ops->changed ? (x)->ops->changed(x) : false)
#define VK(x) (keyed_var_ops.current(x))
#define CK(x) (keyed_var_ops.changed(x))
