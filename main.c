#include <stdio.h>
#include <stdlib.h>
#include <ev.h>
#include <unistd.h>
#include <ini.h>
#include <libudev.h>
#include "font.h"
#include "user.h"
#include "scene.h"
#include "common.h"
#include "backend.h"
#include "render.h"
#include "image.h"
#include "input.h"
#include "interpolate.h"

struct config {
	struct backend *b;
	const struct backend_ops *bops;
	struct interpolate_man *im;
	struct scene *s;
	struct input *i;
	struct font *f;
	struct fb *cursor;

	double last_timestamp;
};

int coral_ini_handler(void *ud, const char *section, const char *name, const char *value) {
	struct config *c = ud;
	if (strcmp(name, "cursor") == 0) {
		c->cursor = load_image(value);
	}
	return 1;
}

void load_config(struct config *cfg) {
	const char *global_config = "/etc/coral.conf";
	if (access(global_config, R_OK) != 0)
		return;

	ini_parse(global_config, coral_ini_handler, (void *)cfg);
}

void render_callback(EV_P_ void *user_data) {
	struct config *c = user_data;
	struct fb *fb = c->bops->new_fb(c->b, RENDER_FB);
	if (!fb)
		ev_break(EV_A_ EVBREAK_ALL);
	interpolate_man_advance(c->im, ev_now(EV_A)-c->last_timestamp);
	c->last_timestamp = ev_now(EV_A);
	render_scene(fb, c->s);
	//fprintf(stderr, "queue frame\n");

	uint32_t x, y;
	libinput_ops.pointer_coord(c->i, &x, &y);
	c->bops->queue_frame(c->b, fb, x, y);
}

void mouse_button_cb(int button, uint16_t state, bool pressed, void *ud) {

}

void mouse_move_cb(uint32_t x, uint32_t y, void *ud) {
	struct config *c = ud;
	handle_mouse_move(c->s, x, y);
}

int main() {
	struct config cfg = {0};
	load_config(&cfg);
	cfg.im = interpolate_man_new();
	cfg.f = init_font();
	load_font(cfg.f, "Helvetica Neue Regular");
	if (!cfg.f) {
		fprintf(stderr, "Could not initialize font\n");
		return -1;
	}

	auto u = udev_new();
	if (!u) {
		fprintf(stderr, "Could not initialize udev\n");
		return -1;
	}

	size_t nusers;
	auto users = load_users(&nusers);

	cfg.bops = &drm_ops;
	// w, h is ignored right now. Do we really need that?
	cfg.b = cfg.bops->setup(EV_DEFAULT, u, 0, 0);
	if (!cfg.b)
		return -1;

	cfg.i = libinput_ops.setup(EV_DEFAULT, u, cfg.b->w, cfg.b->h);
	cfg.i->user_data = &cfg;
	cfg.i->mouse_button_cb = mouse_button_cb;
	cfg.i->mouse_move_cb = mouse_move_cb;

	if (!cfg.cursor) {
		cfg.cursor = new_fb(32, 32, ARGB8888);
		memset(cfg.cursor->data, 255, cfg.cursor->pitch*cfg.cursor->height);
	}
	cfg.bops->set_cursor(cfg.b, cfg.cursor);

	cfg.s = build_scene(cfg.im, users, nusers, cfg.b->w, cfg.b->h);
	if (!cfg.s)
		return 1;
	cfg.b->user_data = &cfg;
	cfg.b->page_flip_cb = render_callback;
	cfg.last_timestamp = ev_now(EV_DEFAULT);

	// Render first frame
	struct fb *fb = cfg.bops->new_fb(cfg.b, RENDER_FB);
	if (!fb)
		return 1;
	cfg.last_timestamp = ev_now(EV_DEFAULT);
	render_scene(fb, cfg.s);

	uint32_t x, y;
	libinput_ops.pointer_coord(cfg.i, &x, &y);
	cfg.bops->queue_frame(cfg.b, fb, x, y);
	ev_run(EV_DEFAULT, 0);

	return 0;
}
