#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "common.h"

struct font {
	FT_Library ft;
};

struct font *init_font(void) {
	if (!FcInit())
		return NULL;

	auto f = tmalloc(struct font, 1);
	auto err = FT_Init_FreeType(&f->ft);
	if (err) {
		free(f);
		return NULL;
	}

	return f;
}
