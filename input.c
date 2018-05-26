#include <ev.h>
#include <stdio.h>
#include <stdlib.h>
#include <libinput.h>
#include <libudev.h>
#include <fcntl.h>
#include <unistd.h>
#include "coral.h"
#include "input.h"
#include "common.h"

struct libinput_input {
	struct input base;
	uint16_t kbstate;
};

typedef struct {
	ev_io b;
	struct libinput_input *li;
	struct libinput *libinput;
} ev_io_libinput;

static int open_restricted(const char *path, int flags, void *ud) {
	return open(path, flags);
}

static void close_restricted(int fd, void *ud) {
	close(fd);
}

static void process_events(ev_io_libinput *w) {
	struct input *i = &w->li->base;
	struct libinput_event *ev;
	while ((ev = libinput_get_event(w->libinput))) {
		if (libinput_event_get_type(ev) == LIBINPUT_EVENT_DEVICE_ADDED) {
			fprintf(stderr, "New device\n");
			auto dev = libinput_event_get_device(ev);
			if (libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_KEYBOARD) ||
			    libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_POINTER))
				libinput_device_config_send_events_set_mode(dev, LIBINPUT_CONFIG_SEND_EVENTS_ENABLED);
			else
				libinput_device_config_send_events_set_mode(dev, LIBINPUT_CONFIG_SEND_EVENTS_DISABLED);
		} else if (libinput_event_get_type(ev) == LIBINPUT_EVENT_POINTER_MOTION && i->mouse_move_rel_cb) {
			auto pt = libinput_event_get_pointer_event(ev);
			i->mouse_move_rel_cb(libinput_event_pointer_get_dx(pt),
			                     libinput_event_pointer_get_dy(pt), i->user_data);
		} else if (libinput_event_get_type(ev) == LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE && i->mouse_move_abs_cb) {
			auto pt = libinput_event_get_pointer_event(ev);
			i->mouse_move_abs_cb(libinput_event_pointer_get_absolute_x(pt),
			                     libinput_event_pointer_get_absolute_y(pt), i->user_data);
		}
	}
}

static void input_cb(EV_P_ ev_io *_w, int revent) {
	ev_io_libinput *w = (void*)_w;
	libinput_dispatch(w->libinput);
	process_events(w);
}
struct input *setup_libinput(EV_P, struct udev *u) {
	auto li = libinput_udev_create_context(
	    (struct libinput_interface[]){{open_restricted, close_restricted}}, NULL, u);
	libinput_udev_assign_seat(li, "seat0");

	auto lii =  tmalloc(struct libinput_input, 1);
	auto iw = tmalloc(ev_io_libinput, 1);
	iw->li = lii;
	iw->libinput = li;
	process_events(iw);

	ev_io_init((ev_io *)iw, input_cb, libinput_get_fd(li), EV_READ);
	ev_io_start(EV_A_ (ev_io *)iw);

	return &lii->base;
}

const struct input_ops libinput_ops = {
	.setup = setup_libinput,
	.set_kb_layout = NULL,
};
