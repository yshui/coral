#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "user.h"
#include "common.h"

static const char avatar_file[] = "/.face.icon";

struct user *load_users(size_t *nusers) {
	size_t cap = 10, n = 0;
	auto ret = tmalloc(struct user, cap);
	struct passwd *p;
	while((p = getpwent())) {
		if (n >= cap) {
			cap *= 2;
			ret = realloc(ret, cap*sizeof(struct user));
		}
		ret[n].name = strdup(p->pw_name);
		ret[n++].uid = p->pw_uid;

		size_t dirlen = strlen(p->pw_dir);
		char *avatar_path = malloc(dirlen+strlen(avatar_file)+1);
		strcpy(avatar_path, p->pw_dir);
		strcpy(avatar_path+dirlen, avatar_file);

		FILE *f = fopen(avatar_path, "r");
		if (!f) {
			free(avatar_path);
			continue;
		}
		int x, y, c;
		auto img = stbi_load_from_callbacks(&stbi__stdio_callbacks, f, &x, &y, &c, 4);
		fclose(f);
		free(avatar_path);

		if (!img)
			continue;

		auto a = tmalloc(struct fb, 1);
		a->height = y;
		a->width = x;
		a->pitch = XRGB8888;
		a->pitch = x*4;
		a->data = img;
		ret[n-1].avatar = a;
	}
	if (cap != n)
		ret = realloc(ret, n*sizeof(struct user));
	*nusers = n;
	return ret;
}
