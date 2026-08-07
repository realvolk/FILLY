#include "vector.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

typedef struct { unsigned char type; short x, y, cx, cy, cx1, cy1; } stbtt_vertex;
typedef struct { unsigned char *pixels; int w, h, stride; } stbtt__bitmap;

#define STBTT_vmove 1
#define STBTT_vline 2
#define STBTT_vcurve 3
#define STBTT_vcubic 4

void stbtt_Rasterize(stbtt__bitmap *result, float flatness_in_pixels, stbtt_vertex *vertices, int num_verts, float scale_x, float scale_y, float shift_x, float shift_y, int x_off, int y_off, int invert, void *userdata);

typedef struct {
    float x, y;
} Vec2;

typedef struct {
    Vec2 pos;
    Vec2 start;
    bool started;
    stbtt_vertex *verts;
    int vert_count;
    int vert_capacity;
    int *contours;
    int contour_count;
    int contour_capacity;
} PathBuilder;

static void pb_init(PathBuilder *pb) {
    memset(pb, 0, sizeof(*pb));
    pb->vert_capacity = 256;
    pb->verts = malloc(pb->vert_capacity * sizeof(stbtt_vertex));
    pb->contour_capacity = 16;
    pb->contours = malloc(pb->contour_capacity * sizeof(int));
}

static void pb_free(PathBuilder *pb) {
    free(pb->verts);
    free(pb->contours);
    memset(pb, 0, sizeof(*pb));
}

static void pb_ensure(PathBuilder *pb, int needed) {
    while (pb->vert_count + needed > pb->vert_capacity) {
        pb->vert_capacity *= 2;
        pb->verts = realloc(pb->verts, pb->vert_capacity * sizeof(stbtt_vertex));
    }
}

static void pb_add_vertex(PathBuilder *pb, uint8_t type, float x, float y, float cx, float cy) {
    pb_ensure(pb, 1);
    stbtt_vertex *v = &pb->verts[pb->vert_count++];
    v->type = type;
    v->x = (short)x;
    v->y = (short)y;
    v->cx = (short)cx;
    v->cy = (short)cy;
    v->cx1 = 0;
    v->cy1 = 0;
}

static void pb_add_cubic(PathBuilder *pb, uint8_t type, float x, float y, float cx1, float cy1, float cx2, float cy2) {
    pb_ensure(pb, 1);
    stbtt_vertex *v = &pb->verts[pb->vert_count++];
    v->type = type;
    v->x = (short)x;
    v->y = (short)y;
    v->cx = (short)cx1;
    v->cy = (short)cy1;
    v->cx1 = (short)cx2;
    v->cy1 = (short)cy2;
}

static void pb_end_contour(PathBuilder *pb) {
    if (!pb->started) return;
    pb_add_vertex(pb, STBTT_vline, pb->start.x, pb->start.y, 0, 0);
    pb->started = false;
    if (pb->contour_count >= pb->contour_capacity) {
        pb->contour_capacity *= 2;
        pb->contours = realloc(pb->contours, pb->contour_capacity * sizeof(int));
    }
    int prev_total = pb->contour_count > 0 ? pb->contours[pb->contour_count - 1] : 0;
    pb->contours[pb->contour_count++] = pb->vert_count - prev_total;
}

static void pb_move_to(PathBuilder *pb, float x, float y) {
    if (pb->started) pb_end_contour(pb);
    pb_add_vertex(pb, STBTT_vmove, x, y, 0, 0);
    pb->start.x = x;
    pb->start.y = y;
    pb->pos.x = x;
    pb->pos.y = y;
    pb->started = true;
}

static void pb_line_to(PathBuilder *pb, float x, float y) {
    pb_add_vertex(pb, STBTT_vline, x, y, 0, 0);
    pb->pos.x = x;
    pb->pos.y = y;
}

static void pb_quad_to(PathBuilder *pb, float cx, float cy, float x, float y) {
    pb_add_vertex(pb, STBTT_vcurve, x, y, cx, cy);
    pb->pos.x = x;
    pb->pos.y = y;
}

static void pb_cubic_to(PathBuilder *pb, float cx1, float cy1, float cx2, float cy2, float x, float y) {
    pb_add_cubic(pb, STBTT_vcubic, x, y, cx1, cy1, cx2, cy2);
    pb->pos.x = x;
    pb->pos.y = y;
}

static float svg_parse_number(const char **d) {
    while (**d == ' ' || **d == ',' || **d == '\t' || **d == '\n' || **d == '\r') (*d)++;
    char *end;
    float val = strtof(*d, &end);
    *d = end;
    return val;
}

static char svg_peek_command(const char **d) {
    while (**d == ' ' || **d == ',' || **d == '\t' || **d == '\n' || **d == '\r') (*d)++;
    return **d;
}

static float reflect(float val, float center) {
    return 2.0f * center - val;
}

VectorPath *vector_parse_svg_path(const char *d, float scale) {
    if (!d || !d[0]) return NULL;
    PathBuilder pb;
    pb_init(&pb);
    Vec2 prev_control = {0, 0};
    Vec2 start_point = {0, 0};
    char cmd = 0;

    while (*d) {
        while (*d == ' ' || *d == ',' || *d == '\t' || *d == '\n' || *d == '\r') d++;
        if (!*d) break;
        char peeked = *d;
        if ((peeked >= 'A' && peeked <= 'Z') || (peeked >= 'a' && peeked <= 'z')) {
            cmd = *d++;
            while (*d == ' ') d++;
        }
        if (!cmd) break;

        bool relative = (cmd >= 'a' && cmd <= 'z');

        switch (cmd) {
            case 'M': case 'm': {
                float x = svg_parse_number(&d) * scale;
                float y = svg_parse_number(&d) * scale;
                if (relative) { x += pb.pos.x; y += pb.pos.y; }
                pb_move_to(&pb, x, y);
                start_point.x = x;
                start_point.y = y;
                cmd = (cmd == 'M') ? 'L' : 'l';
                break;
            }
            case 'L': case 'l': {
                float x = svg_parse_number(&d) * scale;
                float y = svg_parse_number(&d) * scale;
                if (relative) { x += pb.pos.x; y += pb.pos.y; }
                pb_line_to(&pb, x, y);
                break;
            }
            case 'H': case 'h': {
                float x = svg_parse_number(&d) * scale;
                if (relative) x += pb.pos.x;
                pb_line_to(&pb, x, pb.pos.y);
                break;
            }
            case 'V': case 'v': {
                float y = svg_parse_number(&d) * scale;
                if (relative) y += pb.pos.y;
                pb_line_to(&pb, pb.pos.x, y);
                break;
            }
            case 'Q': case 'q': {
                float cx = svg_parse_number(&d) * scale;
                float cy = svg_parse_number(&d) * scale;
                float x = svg_parse_number(&d) * scale;
                float y = svg_parse_number(&d) * scale;
                if (relative) { cx += pb.pos.x; cy += pb.pos.y; x += pb.pos.x; y += pb.pos.y; }
                prev_control.x = cx;
                prev_control.y = cy;
                pb_quad_to(&pb, cx, cy, x, y);
                break;
            }
            case 'T': case 't': {
                float cx = reflect(prev_control.x, pb.pos.x);
                float cy = reflect(prev_control.y, pb.pos.y);
                float x = svg_parse_number(&d) * scale;
                float y = svg_parse_number(&d) * scale;
                if (relative) { x += pb.pos.x; y += pb.pos.y; }
                prev_control.x = cx;
                prev_control.y = cy;
                pb_quad_to(&pb, cx, cy, x, y);
                break;
            }
            case 'C': case 'c': {
                float cx1 = svg_parse_number(&d) * scale;
                float cy1 = svg_parse_number(&d) * scale;
                float cx2 = svg_parse_number(&d) * scale;
                float cy2 = svg_parse_number(&d) * scale;
                float x = svg_parse_number(&d) * scale;
                float y = svg_parse_number(&d) * scale;
                if (relative) { cx1 += pb.pos.x; cy1 += pb.pos.y; cx2 += pb.pos.x; cy2 += pb.pos.y; x += pb.pos.x; y += pb.pos.y; }
                prev_control.x = cx2;
                prev_control.y = cy2;
                pb_cubic_to(&pb, cx1, cy1, cx2, cy2, x, y);
                break;
            }
            case 'S': case 's': {
                float cx1 = reflect(prev_control.x, pb.pos.x);
                float cy1 = reflect(prev_control.y, pb.pos.y);
                float cx2 = svg_parse_number(&d) * scale;
                float cy2 = svg_parse_number(&d) * scale;
                float x = svg_parse_number(&d) * scale;
                float y = svg_parse_number(&d) * scale;
                if (relative) { cx2 += pb.pos.x; cy2 += pb.pos.y; x += pb.pos.x; y += pb.pos.y; }
                prev_control.x = cx2;
                prev_control.y = cy2;
                pb_cubic_to(&pb, cx1, cy1, cx2, cy2, x, y);
                break;
            }
            case 'Z': case 'z': {
                pb_line_to(&pb, start_point.x, start_point.y);
                pb.pos.x = start_point.x;
                pb.pos.y = start_point.y;
                break;
            }
            default: {
                d++;
                break;
            }
        }
    }

    if (pb.started) pb_end_contour(&pb);
    if (pb.contour_count == 0 && pb.vert_count > 0) {
        pb.contours = realloc(pb.contours, sizeof(int));
        pb.contours[0] = pb.vert_count;
        pb.contour_count = 1;
    }

    if (pb.vert_count == 0) {
        pb_free(&pb);
        return NULL;
    }

    VectorPath *path = calloc(1, sizeof(VectorPath));
    path->vertices = malloc(pb.vert_count * 2 * sizeof(float));
    for (int i = 0; i < pb.vert_count; i++) {
        path->vertices[i * 2] = pb.verts[i].x;
        path->vertices[i * 2 + 1] = pb.verts[i].y;
    }
    path->vertex_count = pb.vert_count;
    path->contour_lengths = malloc(pb.contour_count * sizeof(int));
    memcpy(path->contour_lengths, pb.contours, pb.contour_count * sizeof(int));
    path->contour_count = pb.contour_count;

    float min_x = 1e9f, min_y = 1e9f, max_x = -1e9f, max_y = -1e9f;
    for (int i = 0; i < pb.vert_count; i++) {
        if (pb.verts[i].x < min_x) min_x = pb.verts[i].x;
        if (pb.verts[i].y < min_y) min_y = pb.verts[i].y;
        if (pb.verts[i].x > max_x) max_x = pb.verts[i].x;
        if (pb.verts[i].y > max_y) max_y = pb.verts[i].y;
    }
    path->min_x = min_x;
    path->min_y = min_y;
    path->max_x = max_x;
    path->max_y = max_y;

    pb_free(&pb);
    return path;
}

void vector_path_free(VectorPath *path) {
    if (!path) return;
    free(path->vertices);
    free(path->contour_lengths);
    free(path);
}

void vector_get_bounds(VectorPath *path, int *x, int *y, int *w, int *h, float scale) {
    if (!path) { *x = 0; *y = 0; *w = 0; *h = 0; return; }
    *x = (int)floorf(path->min_x * scale);
    *y = (int)floorf(path->min_y * scale);
    *w = (int)ceilf(path->max_x * scale) - *x + 1;
    *h = (int)ceilf(path->max_y * scale) - *y + 1;
}

VectorBitmap *vector_rasterize(VectorPath *path, float scale, int padding, bool invert) {
    (void)invert;
    if (!path || path->vertex_count == 0) return NULL;
    int x, y, w, h;
    vector_get_bounds(path, &x, &y, &w, &h, scale);
    if (w <= 0 || h <= 0) return NULL;
    int pw = w + padding * 2;
    int ph = h + padding * 2;
    VectorBitmap *bmp = calloc(1, sizeof(VectorBitmap));
    bmp->pixels = calloc(pw * ph, 1);
    bmp->width = pw;
    bmp->height = ph;
    bmp->stride = pw;
    vector_rasterize_to_buffer(path, bmp->pixels, pw, ph, pw, scale, -x + padding, -y + padding);
    return bmp;
}

void vector_rasterize_to_buffer(VectorPath *path, uint8_t *output, int out_w, int out_h, int out_stride, float scale, int off_x, int off_y) {
    if (!path || !output || path->vertex_count == 0) return;
    stbtt_vertex *verts = malloc(path->vertex_count * sizeof(stbtt_vertex));
    for (int i = 0; i < path->vertex_count; i++) {
        verts[i].x = (short)(path->vertices[i * 2] * scale);
        verts[i].y = (short)(path->vertices[i * 2 + 1] * scale);
        verts[i].cx = 0;
        verts[i].cy = 0;
        verts[i].cx1 = 0;
        verts[i].cy1 = 0;
        verts[i].type = STBTT_vline;
    }
    int vert_idx = 0;
    for (int c = 0; c < path->contour_count; c++) {
        int count = path->contour_lengths[c];
        if (vert_idx + count > path->vertex_count) break;
        int start = vert_idx;
        verts[start].type = STBTT_vmove;
        for (int i = 1; i < count; i++)
            verts[start + i].type = STBTT_vline;
        vert_idx += count;
    }
    stbtt__bitmap bm;
    bm.pixels = output;
    bm.w = out_w;
    bm.h = out_h;
    bm.stride = out_stride;
    stbtt_Rasterize(&bm, 0.35f, verts, path->vertex_count, 1.0f, 1.0f, 0, 0, off_x, off_y, 1, NULL);
    free(verts);
}

void vector_bitmap_free(VectorBitmap *bmp) {
    if (!bmp) return;
    free(bmp->pixels);
    free(bmp);
}