#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float *vertices;
    int vertex_count;
    int *contour_lengths;
    int contour_count;
    float min_x, min_y, max_x, max_y;
} VectorPath;

typedef struct {
    uint8_t *pixels;
    int width;
    int height;
    int stride;
} VectorBitmap;

VectorPath *vector_parse_svg_path(const char *d, float scale);
void vector_path_free(VectorPath *path);
VectorBitmap *vector_rasterize(VectorPath *path, float scale, int padding, bool invert);
void vector_rasterize_to_buffer(VectorPath *path, uint8_t *output, int out_w, int out_h, int out_stride, float scale, int off_x, int off_y);
void vector_bitmap_free(VectorBitmap *bmp);
void vector_get_bounds(VectorPath *path, int *x, int *y, int *w, int *h, float scale);