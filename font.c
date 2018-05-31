#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "common.h"
#include "font.h"

struct font {
	FcConfig *fc;
	FT_Library ft;
	FcPattern *pat;
};

struct font *init_font(void) {
	auto f = tmalloc(struct font, 1);
	f->fc = FcInitLoadConfigAndFonts();
	if (!f->fc) {
		free(f);
		return NULL;
	}

	auto err = FT_Init_FreeType(&f->ft);
	if (err) {
		free(f);
		return NULL;
	}

	return f;
}

int load_font(struct font *f, const char *pattern) {
	auto pat = FcNameParse((const FcChar8 *)pattern);
	if (!pat)
		return -1;

	auto os = FcObjectSetBuild(FC_INDEX, FC_STYLE, FC_HINTING, FC_AUTOHINT, FC_RGBA, FC_FILE, NULL);
	auto fs = FcFontList(f->fc, pat, os);

	fprintf(stderr, "nfont %d\n", fs->nfont);
	for (int i = 0; i < fs->nfont; i++) {
		FcPatternPrint(fs->fonts[i]);
		//FcPatternGetString(fs->fonts[i])
	}

	return 0;
}
