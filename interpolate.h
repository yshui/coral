#pragma once
#include <stdbool.h>
#include "list.h"
struct var;
struct key_frame {
	double eta;
	double setpoint;
	void (*callback)(struct key_frame *k, bool finished);
	void (*advance)(struct var *i, struct key_frame *k, double dt);
	struct key_frame *next;
};
typedef struct var {
	struct list_head siblings;
	struct key_frame *keys, **last_key;
	double current;
	double last_setpoint;
	double elapsed;
	bool changed;
} var;
struct interpolatable_man;

#define V(x) ((x)->current)
