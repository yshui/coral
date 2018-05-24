#pragma once
#include <unistd.h>
#include <stddef.h>
#include "render.h"
struct user {
	struct fb *avatar;
	const char *name;
	uid_t uid;
};

struct user *load_users(size_t *nusers);
