#pragma once

struct font;
struct font *init_font(void);
int load_font(struct font *, const char *);
