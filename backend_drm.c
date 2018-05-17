#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <ev.h>
#include <assert.h>
#include <sys/mman.h>
#include "common.h"
#include "backend.h"
#include "render.h"
struct fb_params {
	size_t pitch;
	size_t size;
	uint32_t w, h;
	uint8_t * restrict map;
	uint32_t fb;
};
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
struct drm_backend {
	struct backend base;
	ev_io iow;
	drmModeCrtcPtr crtc;
	uint32_t cursor_x, cursor_y;
	int fd;

	// 0,1 double primary fb, 2 cursor fb
	struct fb_params fb[3];
	struct plane plane[2]; // 0 is primary, 1 is cursor
	uint8_t front;
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
	return drmModeAtomicCommit(fd, atomic, flags, NULL);
}
static inline drmModeCrtcPtr find_crtc(int fd, uint32_t w, uint32_t h) {
	auto r = drmModeGetResources(fd);
	if (!r)
		return NULL;

	drmModeCrtcPtr ret = NULL;
	for (int i = 0; i < r->count_connectors; i++) {
		auto c = drmModeGetConnector(fd, r->connectors[i]);
		if (!c)
			break;
		if (c->connection != DRM_MODE_CONNECTED) {
			drmModeFreeConnector(c);
			continue;
		}
		auto e = drmModeGetEncoder(fd, c->encoder_id);
		ret = drmModeGetCrtc(fd, e->crtc_id);
		drmModeFreeConnector(c);
		drmModeFreeEncoder(e);
		break;
	}

	drmModeFreeResources(r);
	return ret;
}

static inline void init_plane_props(int fd, struct plane *p) {
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
__attribute__((unused)) \
static inline uint64_t get_plane_##name(struct plane *pl, drmModeObjectPropertiesPtr props)  { \
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

static int find_plane_by_type(drmModePlaneResPtr pres, int fd, uint64_t wtype, struct plane *res) {
	for (size_t i = 0; i < pres->count_planes; i++) {
		struct plane p;
		auto pl = drmModeGetPlane(fd, pres->planes[i]);
		p.id = pl->plane_id;
		drmModeFreePlane(pl);

		auto props = drmModeObjectGetProperties(fd, p.id, DRM_MODE_OBJECT_ANY);
		if (!props)
			continue;
		init_plane_props(fd, &p);

		auto type = get_plane_type(&p, props);
		drmModeFreeObjectProperties(props);
		if (type == wtype) {
			*res = p;
			return 0;
		}
	}
	return -1;
}

static int setup_plane(drmModeAtomicReqPtr atomic, int fd, struct plane *pl, uint32_t crtc_id) {
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
	atomic_add(atomic, id, props->crtc_id, crtc_id);
	atomic_add(atomic, id, props->crtc_x, 0);
	atomic_add(atomic, id, props->crtc_y, 0);
	if (!atomic_check(atomic, fd))
		return -1;
	return 0;
}

static inline int init_fb(int fd, int w, int h, int depth, struct fb_params *p) {
	struct drm_mode_create_dumb create_req = {0};
	create_req.width = w;
	create_req.height = h;
	create_req.bpp = 32;
	int ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req);
	if (ret)
		return ret;
	//printf("Created dumb: pitch: %d, handle: %d, size: %llu\n", create_req.pitch, create_req.handle, create_req.size);

	ret = drmModeAddFB(fd, w, h, depth, 32, create_req.pitch, create_req.handle, &p->fb);
	if (ret)
		goto free_dumb;

	struct drm_mode_map_dumb map_req = {0};
	map_req.handle = create_req.handle;

	ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req);
	if (ret)
		goto free_dumb;

	//printf("Map request sent: offset: %llu\n", map_req.offset);
	p->map = mmap(0, create_req.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map_req.offset);
	if (p->map == MAP_FAILED)
		goto free_dumb;
	//printf("Mapped: %p\n", p->map);

	p->pitch = create_req.pitch;
	p->size = create_req.size;
	p->w = w;
	p->h = h;
	return 0;

free_dumb:;
	struct drm_mode_destroy_dumb destroy_req = { create_req.handle };
	drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);
	return ret;
}

static void drm_callback(EV_P_ ev_io *iow, int revent) {
	struct drm_backend *b = container_of(iow, struct drm_backend, iow);
	b->base.busy = false;
	if (b->base.page_flip_cb)
		b->base.page_flip_cb(EV_A_ b->base.user_data);
}
struct backend *drm_setup(EV_P_ uint32_t w, uint32_t h) {
#define ERET(expr) do { \
	__auto_type ret = (expr); \
	if (ret) \
		goto err_out; \
} while(0)

	struct drm_backend *b = tmalloc(struct drm_backend, 1);
	drmModeAtomicReqPtr atomic = NULL;

	// TODO iterate over cards
	int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	if (fd < 0)
		return NULL;

	drmDropMaster(fd);
	drmSetMaster(fd);

	uint64_t tmp;
	ERET(drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1));
	ERET(drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &tmp));
	if (tmp == 0)
		goto err_out;

	b->crtc = find_crtc(fd, w, h);
	b->base.w = b->crtc->width;
	b->base.h = b->crtc->height;

	ERET(drmGetCap(fd, DRM_CAP_CURSOR_WIDTH, &tmp));
	b->base.cursor_w = tmp;
	ERET(drmGetCap(fd, DRM_CAP_CURSOR_HEIGHT, &tmp));
	b->base.cursor_h = tmp;

	ERET(init_fb(fd, b->base.w, b->base.h, 24, &b->fb[0]));
	ERET(init_fb(fd, b->base.w, b->base.h, 24, &b->fb[1]));
	ERET(init_fb(fd, b->base.cursor_w, b->base.cursor_h, 24, &b->fb[2]));
	b->front = 0;

	auto pres = drmModeGetPlaneResources(fd);
	ERET(find_plane_by_type(pres, fd, DRM_PLANE_TYPE_PRIMARY, &b->plane[0]));
	ERET(find_plane_by_type(pres, fd, DRM_PLANE_TYPE_CURSOR, &b->plane[1]));
	drmModeFreePlaneResources(pres);

	ev_io_init(&b->iow, drm_callback, fd, EV_READ);
	ev_io_start(EV_A_ &b->iow);

	atomic = atomic_begin();
	ERET(setup_plane(atomic, fd, &b->plane[0], b->crtc->crtc_id));
	ERET(setup_plane(atomic, fd, &b->plane[1], b->crtc->crtc_id));
	ERET(atomic_commit(atomic, fd));
	drmModeAtomicFree(atomic);
	atomic = NULL;

	b->fd = fd;
	return &b->base;
err_out:
	if (atomic)
		drmModeAtomicFree(atomic);
	free(b);
	drmDropMaster(fd);
	close(fd);
	return NULL;
}

static int
drm_queue_frame(struct backend *_b, struct fb *fb,
                uint32_t cursor_x, uint32_t cursor_y) {
	struct drm_backend *b = (void *)_b;
	if (b->base.busy)
		return -1;

	if (fb->pitch != b->fb[0].pitch ||
	    fb->pitch*fb->height != b->fb[0].size ||
	    fb->width != b->fb[0].w ||
	    fb->height != b->fb[0].h)
		return -2;

	memcpy(b->fb[b->front^1].map, fb->data, b->fb[0].size);
	b->front ^= 1;

	auto atomic = atomic_begin();
	atomic_add(atomic, b->plane[0].id, b->plane[0].pid.fb_id, b->fb[b->front].fb);
	atomic_add(atomic, b->plane[1].id, b->plane[1].pid.crtc_x, cursor_x);
	atomic_add(atomic, b->plane[1].id, b->plane[1].pid.crtc_y, cursor_y);
	ERET(atomic_check(atomic, b->fd));
	ERET(atomic_commit(atomic, b->fd));
	drmModeAtomicFree(atomic);
	atomic = NULL;

	free(fb->data);
	free(fb);
	return 0;

err_out:
	if (atomic)
		drmModeAtomicFree(atomic);
	return -3;
#undef ERET
}

static bool
drm_set_cursor(struct backend *_b, struct fb *fb) {
	struct drm_backend *b = (void *)_b;
	if (fb->pitch != b->fb[2].pitch ||
	    fb->pitch*fb->height != b->fb[2].size ||
	    fb->width != b->fb[2].w ||
	    fb->height != b->fb[2].h)
		return false;
	memcpy(b->fb[2].map, fb->data, b->fb[2].size);
	return true;
}

static struct fb *
drm_new_fb(struct backend *_b) {
	struct drm_backend *b = (void *)_b;
	auto ret = tmalloc(struct fb, 1);
	ret->bpp = 4;
	ret->pitch = b->fb[0].pitch;
	ret->height = b->fb[0].h;
	ret->width = b->fb[0].w;
	ret->data = calloc(ret->pitch, ret->height);
	return ret;
}

const struct backend_ops drm_ops = {
	.setup = drm_setup,
	.queue_frame = drm_queue_frame,
	.set_cursor = drm_set_cursor,
	.new_fb = drm_new_fb
};
