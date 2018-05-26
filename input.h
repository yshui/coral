#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <ev.h>
struct udev;
struct input {
	void *user_data;
	void (*mouse_move_rel_cb)(int dx, int dy, void *user_data);
	void (*mouse_move_abs_cb)(uint32_t x, uint32_t y, void *user_data);
	void (*mouse_button_cb)(int button, bool pressed, void *user_data);
	void (*key_cb)(int key, uint16_t state, bool pressed, void *user_data);
};

struct input_ops {
	struct input *(*setup)(EV_P, struct udev *);
	bool (*set_kb_layout)(struct input *, const char *layout);
};

extern const struct input_ops libinput_ops;
