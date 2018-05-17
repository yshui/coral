#pragma once
#include <stdint.h>
#include <ev.h>
struct input {
	uint32_t cursor_x, cursor_y;
};
struct input *setup_input(EV_P);
