#include <xf86drm.h>
#include <xf86drmMode.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <sys/mman.h>
#include <string.h>
#include <libinput.h>
#include <libudev.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <ev.h>
#define auto __auto_type
#define ARR_LEN(a) (sizeof(a)/sizeof((a)[0]))

struct fb_params {
	size_t pitch;
	size_t size;
	uint32_t w, h;
	uint8_t * restrict map;
	uint32_t fb;
};

typedef struct {
	ev_io b;
	struct libinput *libinput;
	int drm_fd;
} ev_io_libinput;

struct plane_prop_ids {
	uint32_t src_x, src_y, src_w, src_h;
	uint32_t crtc_x, crtc_y, crtc_w, crtc_h;
	uint32_t fb_id, crtc_id;
	uint32_t type;
};

struct plane {
	int id;
	struct fb_params *curr_fb;
	struct plane_prop_ids pid;
};

struct prop_info {
	const char *key;
	uintptr_t offset;
};

static const struct prop_info plane_info[] = {
#define O(name) (offsetof(struct plane_prop_ids, name))
	{ "CRTC_H",  O(crtc_h) },
	{ "CRTC_ID", O(crtc_id) },
	{ "CRTC_W",  O(crtc_w) },
	{ "CRTC_X",  O(crtc_x) },
	{ "CRTC_Y",  O(crtc_y) },
	{ "FB_ID",   O(fb_id) },
	{ "SRC_H",   O(src_h) },
	{ "SRC_W",   O(src_w) },
	{ "SRC_X",   O(src_x) },
	{ "SRC_Y",   O(src_y) },
	{ "type",    O(type) },
#undef O
};

static int cmp_prop_info(const void *arg1, const void *arg2) {
	const char *key = arg1;
	const struct prop_info *elem = arg2;

	return elem->key ? strcmp(key, elem->key) : -1;
}

static struct fb_params fbp[2];
static int curr_fb = 0;
static drmModeCrtcPtr use_crtc;
static int color = 255;
static uint32_t x, y;
static struct plane cursor_plane, primary_plane;

#if 0
int uterm_drm_video_find_crtc(drmModeRes *res, drmModeEncoder *enc) {
	int i, crtc;
	struct uterm_display *iter;
	struct uterm_drm_display *ddrm;
	struct shl_dlist *it;

	for (i = 0; i < res->count_crtcs; ++i) {
		if (enc->possible_crtcs & (1 << i)) {
			crtc = res->crtcs[i];
		}
	}

	return -1;
}
#endif

int open_restricted(const char *path, int flags, void *ud) {
	return open(path, flags);
}

void close_restricted(int fd, void *ud) {
	close(fd);
}

static inline drmModeAtomicReqPtr atomic_begin(void) {
	return drmModeAtomicAlloc();
}

static inline void atomic_add(drmModeAtomicReqPtr r, uint32_t id, uint32_t prop, uint64_t val) {
	drmModeAtomicAddProperty(r, id, prop, val);
}

static inline int atomic_check(drmModeAtomicReqPtr atomic, int fd) {
	uint32_t flags = DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_NONBLOCK;
	return drmModeAtomicCommit(fd, atomic, flags, NULL);
}
static inline int atomic_commit(drmModeAtomicReqPtr atomic, int fd) {
	uint32_t flags = DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK;
	int ret = drmModeAtomicCommit(fd, atomic, flags, NULL);
	drmModeAtomicFree(atomic);
	return ret;
}
static void setup_plane(drmModeAtomicReqPtr atomic, int fd, struct plane *pl, uint32_t xx, uint32_t yy) {
	uint32_t id = pl->id;
	auto props = &pl->pid;
	auto src_w = pl->curr_fb->w;
	auto src_h = pl->curr_fb->h;
	atomic_add(atomic, id, props->src_x, 0);
	atomic_add(atomic, id, props->src_y, 0);
	atomic_add(atomic, id, props->src_w, src_w << 16);
	atomic_add(atomic, id, props->src_h, src_h << 16);
	atomic_add(atomic, id, props->crtc_w, src_w);
	atomic_add(atomic, id, props->crtc_h, src_h);
	atomic_add(atomic, id, props->fb_id, pl->curr_fb->fb);
	atomic_add(atomic, id, props->crtc_id, use_crtc->crtc_id);
	atomic_add(atomic, id, props->crtc_x, xx);
	atomic_add(atomic, id, props->crtc_y, yy);
	assert(!atomic_check(atomic, fd));
}

void swap(drmModeAtomicReqPtr atomic, int fd, struct plane *primary) {
	primary->curr_fb = &fbp[curr_fb^1];
	atomic_add(atomic, primary->id, primary->pid.fb_id, primary->curr_fb->fb);
	curr_fb ^= 1;

	assert(!atomic_check(atomic, fd));
}

static void update_cursor(drmModeAtomicReqPtr atomic, int fd, struct plane *cursor) {
	uint32_t id = cursor->id;
	auto props = &cursor->pid;
	atomic_add(atomic, id, props->crtc_x, x);
	atomic_add(atomic, id, props->crtc_y, y);
	assert(!atomic_check(atomic, fd));
}

static void init_plane_props(int fd, struct plane *p) {
	auto ps = drmModeObjectGetProperties(fd, p->id, DRM_MODE_OBJECT_PLANE);
	assert(ps);
	for (uint32_t i = 0; i < ps->count_props; i++) {
		auto prop = drmModeGetProperty(fd, ps->props[i]);
		struct prop_info *pi = bsearch(prop->name, plane_info,
		    ARR_LEN(plane_info), sizeof(plane_info[0]), cmp_prop_info);
		if (!pi) {
			fprintf(stderr, "unknow prop %s\n", prop->name);
			continue;
		}
		*(uint32_t*)(((char*)&p->pid)+pi->offset) = prop->prop_id;
		drmModeFreeProperty(prop);
	}
	drmModeFreeObjectProperties(ps);
}

#define gen_prop_getter(name) \
static inline uint64_t get_plane_##name(struct plane *pl, drmModeObjectPropertiesPtr props) { \
	for (uint32_t i = 0; i < props->count_props; i++) { \
		if (props->props[i] == pl->pid.name) { \
			fprintf(stderr, "plane "#name": %lu\n", props->prop_values[i]); \
			return props->prop_values[i]; \
		} \
	} \
	assert(false); \
} \

gen_prop_getter(crtc_id)
gen_prop_getter(type)

static struct plane find_plane_by_type(drmModePlaneResPtr pres, int fd, uint64_t wtype) {
	for (size_t i = 0; i < pres->count_planes; i++) {
		auto pl = drmModeGetPlane(fd, pres->planes[i]);
		auto props = drmModeObjectGetProperties(fd, pl->plane_id, DRM_MODE_OBJECT_ANY);
		assert(props);
		struct plane p;
		p.id = pl->plane_id;
		init_plane_props(fd, &p);

		//auto crtc_id = get_plane_crtc_id(&p, props);
		auto type = get_plane_type(&p, props);
		drmModeFreeObjectProperties(props);
		drmModeFreePlane(pl);
		//if (wtype == 1 && crtc_id == use_crtc->crtc_id)
		//	continue;
		if (type == wtype) {
			fprintf(stderr, "returning\n");
			return p;
		}
	}
	assert(false);
}

void page_flip_cb(int fd, unsigned int seq, unsigned int sec, unsigned int usec, void *ud) {
	auto atomic = atomic_begin();
	update_cursor(atomic, fd, &cursor_plane);
	swap(atomic, fd, &primary_plane);
	int ret = atomic_commit(atomic, fd);
	assert(!ret);

	uint8_t *map = __builtin_assume_aligned(fbp[curr_fb^1].map, 16);
	int tmp = color > 0 ? color : -color;
	for (size_t i = 0; i < fbp[0].size/4; i++) {
		map[i*4] = tmp;
		map[i*4+1] = 0;
		map[i*4+2] = 0;
		map[i*4+3] = 0;
	}
	color--;
	if (color <= -256)
		color = 255;
}

void drm_cb(EV_P_ ev_io *w, int revent) {
	drmEventContext ev = {0};
	ev.version = DRM_EVENT_CONTEXT_VERSION;
	ev.page_flip_handler = page_flip_cb;
	int ret = drmHandleEvent(w->fd, &ev);
	assert(!ret);
}

static inline uint32_t clamped_add(uint32_t a, int b, uint32_t upper) {
	assert(a <= upper);
	if (b < 0 && (uint32_t)-b > a)
		return 0;
	if (b > 0 && (uint32_t)b > upper-a)
		return upper;
	return a+b;
}

void input_cb(EV_P_ ev_io *_w, int revent) {
	ev_io_libinput *w = (void*)_w;
	libinput_dispatch(w->libinput);

	struct libinput_event *ev;
	while ((ev = libinput_get_event(w->libinput))) {
		if (libinput_event_get_type(ev) == LIBINPUT_EVENT_DEVICE_ADDED) {
			fprintf(stderr, "new device\n");
			auto dev = libinput_event_get_device(ev);
			if (libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_KEYBOARD) ||
			    libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_POINTER))
				libinput_device_config_send_events_set_mode(dev, LIBINPUT_CONFIG_SEND_EVENTS_ENABLED);
			else
				libinput_device_config_send_events_set_mode(dev, LIBINPUT_CONFIG_SEND_EVENTS_DISABLED);
		} else if (libinput_event_get_type(ev) == LIBINPUT_EVENT_POINTER_MOTION) {
			auto pt = libinput_event_get_pointer_event(ev);
			x = clamped_add(x, libinput_event_pointer_get_dx(pt), use_crtc->width);
			y = clamped_add(y, libinput_event_pointer_get_dy(pt), use_crtc->height);
			//update_cursor(w->drm_fd);
			fprintf(stderr, "pointer %d %d\n", x, y);
		} else if (libinput_event_get_type(ev) == LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE) {
			auto pt = libinput_event_get_pointer_event(ev);
			x = libinput_event_pointer_get_absolute_x(pt);
			y = libinput_event_pointer_get_absolute_y(pt);
			//update_cursor(w->drm_fd);
			fprintf(stderr, "pointer %d %d\n", x, y);
		}
	}
}

void init_fb(int fd, int w, int h, int depth, struct fb_params *p) {
	struct drm_mode_create_dumb create_req = {0};
	create_req.width = w;
	create_req.height = h;
	create_req.bpp = 32;
	int ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req);
	assert(ret >= 0);
	printf("Created dumb: pitch: %d, handle: %d, size: %llu\n", create_req.pitch, create_req.handle, create_req.size);

	ret = drmModeAddFB(fd, w, h, depth, 32, create_req.pitch, create_req.handle, &p->fb);
	assert(!ret);

	struct drm_mode_map_dumb map_req = {0};
	map_req.handle = create_req.handle;


	ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req);
	assert(!ret);

	printf("Map request sent: offset: %llu\n", map_req.offset);
	p->map = mmap(0, create_req.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map_req.offset);
	assert (p->map != MAP_FAILED);
	printf("Mapped: %p\n", p->map);

	p->pitch = create_req.pitch;
	p->size = create_req.size;
	p->w = w;
	p->h = h;
}

int main() {
	int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	drmDropMaster(fd);
	drmSetMaster(fd);

	int ret = drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);
	assert(!ret);

	auto u = udev_new();
	auto li = libinput_udev_create_context(
	    (struct libinput_interface[]){{open_restricted, close_restricted}}, NULL, u);
	libinput_udev_assign_seat(li, "seat0");

	ev_io drmw;
	ev_io_libinput iw;
	iw.libinput = li;
	iw.drm_fd = fd;
	ev_io_init(&drmw, drm_cb, fd, EV_READ);
	ev_io_init((ev_io *)&iw, input_cb, libinput_get_fd(li), EV_READ);
	ev_io_start(EV_DEFAULT, &drmw);
	ev_io_start(EV_DEFAULT, (ev_io*)&iw);

	auto r = drmModeGetResources(fd);
	for (int i = 0; i < r->count_connectors; i++) {
		auto c = drmModeGetConnector(fd, r->connectors[i]);
		if (c->connection != DRM_MODE_CONNECTED)
			continue;
		auto e = drmModeGetEncoder(fd, c->encoder_id);
		auto crtc = drmModeGetCrtc(fd, e->crtc_id);
		printf("Found first connected display: %dx%d\n", crtc->width, crtc->height);

		use_crtc = crtc;
		drmModeFreeConnector(c);
		drmModeFreeEncoder(e);
		break;
	}
	drmModeFreeResources(r);

	fprintf(stderr, "crtc: %d\n", use_crtc->crtc_id);
	x = use_crtc->width/2;
	y = use_crtc->height/2;
	init_fb(fd, use_crtc->width, use_crtc->height, 24, fbp);
	init_fb(fd, use_crtc->width, use_crtc->height, 24, fbp+1);

	uint8_t *map = fbp[0].map;
	for (size_t i = 0; i < fbp[0].size/4; i++) {
		map[i*4] = 255;
		map[i*4+1] = 0;
		map[i*4+2] = 0;
		map[i*4+3] = 0;
	}

	map = fbp[1].map;
	for (size_t i = 0; i < fbp[0].size/4; i++) {
		map[i*4] = 254;
		map[i*4+1] = 0;
		map[i*4+2] = 0;
		map[i*4+3] = 0;
	}
	color = 253;

	auto pres = drmModeGetPlaneResources(fd);
	cursor_plane = find_plane_by_type(pres, fd, 2);
	primary_plane = find_plane_by_type(pres, fd, 1);

	uint64_t cw, ch;
	ret = drmGetCap(fd, DRM_CAP_CURSOR_WIDTH, &cw);
	cw = ret ? 64 : cw;
	ret = drmGetCap(fd, DRM_CAP_CURSOR_HEIGHT, &ch);
	ch = ret ? 64 : ch;

	struct fb_params *cfbp = malloc(sizeof(*cfbp));
	init_fb(fd, cw, ch, 32, cfbp);
	map = cfbp->map;
	for (size_t i = 0; i < cfbp->size/4; i++) {
		map[i*4] = 0;
		map[i*4+1] = 0;
		map[i*4+2] = 255;
		map[i*4+3] = 255;
	}

	cursor_plane.curr_fb = cfbp;
	primary_plane.curr_fb = &fbp[curr_fb];

	auto atomic = atomic_begin();
	setup_plane(atomic, fd, &primary_plane, 0, 0);
	setup_plane(atomic, fd, &cursor_plane, x, y);
	ret = atomic_commit(atomic, fd);
	assert(!ret);

	ev_run(EV_DEFAULT, 0);

	drmDropMaster(fd);
}
