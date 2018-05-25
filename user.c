#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "user.h"
#include "common.h"
#include "image.h"

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

		ret[n-1].avatar = load_image(avatar_path);
		free(avatar_path);
	}
	if (cap != n)
		ret = realloc(ret, n*sizeof(struct user));
	*nusers = n;
	return ret;
}
