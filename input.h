#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <ev.h>
struct udev;
struct input {
	void *user_data;
	void (*mouse_move_cb)(uint32_t x, uint32_t y, void *user_data);
	void (*mouse_button_cb)(int button, uint16_t state, bool pressed, void *user_data);
	void (*key_cb)(int key, uint16_t state, bool pressed, void *user_data);
};

struct input_ops {
	struct input *(*setup)(EV_P, struct udev *, uint32_t, uint32_t);
	void (*pointer_coord)(struct input *, uint32_t *, uint32_t *);
	bool (*set_kb_layout)(struct input *, const char *layout);
};

extern const struct input_ops libinput_ops;
