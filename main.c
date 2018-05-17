#include <ev.h>
#include "scene.h"
#include "common.h"
#include "backend.h"
int main() {
	auto scene = build_scene();
	if (!scene)
		return 1;
	// w, h is ignored right now. Do we really need that?
	auto backend = drm_ops.setup(EV_DEFAULT, 0, 0);
	if (!backend)
		return 1;

	return 0;
}
