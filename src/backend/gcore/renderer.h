#pragma once
#include "core/render.h"
#include "core/arena.h"
#include "core/theme.h"
#include <stdint.h>

typedef struct {
    uint32_t *pixels;
    int width;
    int height;
    int stride;
    int mouse_x;
    int mouse_y;
} PixelBuffer;

extern bool gcore_font_loaded;
void draw_text_pixel(PixelBuffer *pb, int x, int y, const char *text, int size, uint32_t color);
bool gcore_init_font(const char *font_path, int default_size);
void gcore_shutdown_font(void);
void gcore_render_tree_to_pixels(RenderTree *tree, PixelBuffer *pb, Theme *theme, Arena *arena);
uint32_t gcore_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);