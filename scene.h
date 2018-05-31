#pragma once
#include <stdint.h>
#include <stddef.h>
struct interpolate_man;
struct user;
struct scene;
struct scene *build_scene(struct interpolate_man *, struct user *, size_t nusers, uint32_t, uint32_t);

// x -> x-th scanline from top, y -> y-th pixel from left
void handle_mouse_move(struct scene *, uint32_t x, uint32_t y);
void handle_mouse_button(struct scene *, uint32_t x, uint32_t y, uint16_t state, int16_t button);
