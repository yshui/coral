#include <stdio.h>
#include <stdlib.h>
#include <ev.h>
#include <unistd.h>
#include <ini.h>
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
	uint32_t cursor_x, cursor_y;
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
	render_scene(fb, c->s, false);
	//fprintf(stderr, "queue frame\n");
	c->bops->queue_frame(c->b, fb, c->cursor_x, c->cursor_y);
}

void mouse_button_cb(int button, bool pressed, void *ud) {

}

void mouse_move_rel_cb(int dx, int dy, void *ud) {
	struct config *c = ud;
	if (dx < 0 && c->cursor_x < -dx)
		c->cursor_x = 0;
	else if (dx > 0 && c->b->w-c->cursor_x < dx)
		c->cursor_x = c->b->w;
	else
		c->cursor_x += dx;

	if (dy < 0 && c->cursor_y < -dy)
		c->cursor_y = 0;
	else if (dy > 0 && c->b->h-c->cursor_y < dy)
		c->cursor_y = c->b->h;
	else
		c->cursor_y += dy;
	fprintf(stderr, "%d %d %d %d\n", dx, dy, c->cursor_x, c->cursor_y);
}

int main() {
	struct config cfg;
	load_config(&cfg);
	cfg.im = interpolate_man_new();

	size_t nusers;
	auto users = load_users(&nusers);

	cfg.i = libinput_ops.setup(EV_DEFAULT);
	cfg.i->user_data = &cfg;
	cfg.i->mouse_button_cb = mouse_button_cb;
	cfg.i->mouse_move_rel_cb = mouse_move_rel_cb;

	cfg.bops = &drm_ops;
	// w, h is ignored right now. Do we really need that?
	cfg.b = cfg.bops->setup(EV_DEFAULT, 0, 0);
	if (!cfg.b)
		return 1;

	if (cfg.cursor)
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
	render_scene(fb, cfg.s, true);
	cfg.bops->queue_frame(cfg.b, fb, cfg.cursor_x, cfg.cursor_y);
	ev_run(EV_DEFAULT, 0);

	return 0;
}
