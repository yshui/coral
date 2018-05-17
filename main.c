#include <ev.h>
#include "scene.h"
#include "common.h"
#include "backend.h"
#include "render.h"
#include "input.h"
#include "interpolate.h"

struct config {
	struct backend *b;
	struct backend_ops *bops;
	struct interpolate_man *im;
	struct scene *s;
	struct input *i;

	double last_timestamp;
};

void render_callback(EV_P_ void *user_data) {
	struct config *c = user_data;
	struct fb *fb = c->bops->new_fb(c->b);
	if (!fb) {
		ev_break(EV_A_ EVBREAK_ALL);
	}
	interpolate_man_advance(c->im, ev_now(EV_A)-c->last_timestamp);
	c->last_timestamp = ev_now(EV_A);
	render_scene(fb, c->s);
	c->bops->queue_frame(c->b, fb, c->i->cursor_x, c->i->cursor_y);
}
int main() {
	struct config cfg;
	cfg.im = interpolate_man_new();
	// w, h is ignored right now. Do we really need that?
	cfg.b = drm_ops.setup(EV_DEFAULT, 0, 0);
	if (!cfg.b)
		return 1;
	cfg.s = build_scene(cfg.im, cfg.b->w, cfg.b->h);
	if (!cfg.s)
		return 1;
	cfg.b->page_flip_cb = render_callback;
	cfg.last_timestamp = ev_now(EV_DEFAULT);

	render_callback(EV_DEFAULT, &cfg);
	ev_run(EV_DEFAULT, 0);

	return 0;
}
