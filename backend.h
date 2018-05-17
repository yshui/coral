#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <ev.h>

struct fb;
struct backend {
	bool busy; // indicates whether we could call queue_frame()
	uint32_t w, h, cursor_w, cursor_h;
	void *user_data;
	void (*page_flip_cb)(EV_P_ void *user_data);
};

enum fb_purpose_t {
	RENDER_FB, // Used for rendering, fast CPU access
	PAGE_FB    // Used for sending frame to display
};

struct backend_ops {
	// Setup the backend, if *w and *h != 0, they are
	// the requested width and height.
	//
	// Real width and height is stored into w and h
	struct backend *(*setup)(EV_P, uint32_t w, uint32_t h);

	// Queue one frame, it shoud be submitted to output
	// immediately, and page_flip_cb() should be called
	// when queued frame is presented.
	//
	// Return: -1 bust, -2 invalid fb
	int (*queue_frame)(struct backend *b, struct fb *fb,
	                    uint32_t cursor_x, uint32_t cursor_y);

	// fb has to be exactly cursor_w * cursor_h
	bool (*set_cursor)(struct backend *b, struct fb *fb);

	// Create a fb that's suitable as to be used as a frame
	struct fb *(*new_fb)(struct backend *b, int purpose);
};

extern const struct backend_ops drm_ops;
