#include <ev.h>
#include <stdlib.h>
#include "input.h"
#include "common.h"
struct input *setup_input(EV_P) {
	return tmalloc(struct input, 1);
}
