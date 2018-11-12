#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "common.h"
#include "font.h"

struct font {
	FcConfig *fc;
	FT_Library ft;
	FT_Face face;
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

	FcConfigSubstitute(f->fc, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);

	char* fontFile;
	FcResult result;
	// Use fontconfig to find the "best match"
	FcPattern* font = FcFontMatch(f->fc, pat, &result);
	FcPatternDestroy(pat);

	if (!font)
		return -1;

	FcChar8* file = NULL;
	if (FcPatternGetString(font, FC_FILE, 0, &file) != FcResultMatch)
		return -1;
	int index = 0;
	if (FcPatternGetInteger(font, FC_INDEX, 0, &index) != FcResultMatch)
		return -1;

	// We found the font.
	// This might be a fallback font, though
	fontFile = (char*)file;
	printf("%s\n",fontFile);

	int err = FT_New_Face(f->ft, fontFile, index, &f->face);
	printf("%d\n", err);

	return 0;
}
