#pragma once
#include <stdint.h>
struct interpolate_man;
struct user;
struct scene *build_scene(struct interpolate_man *, struct user *, size_t nusers, uint32_t, uint32_t);
