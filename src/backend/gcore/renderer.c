#include "renderer.h"
#include "core/shaper.h"
#include "core/vector.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

typedef int stbrp_coord;
typedef struct { unsigned char x; } stbrp_node;
typedef struct { int width, height, x, y, bottom_y; } stbrp_context;
typedef struct { stbrp_coord x, y; int id, w, h, was_packed; } stbrp_rect;

static void stbrp_init_target(stbrp_context *con, int pw, int ph, stbrp_node *nodes, int num_nodes) {
    con->width = pw; con->height = ph;
    con->x = 0; con->y = 0; con->bottom_y = 0;
    (void)nodes; (void)num_nodes;
}

static void stbrp_pack_rects(stbrp_context *con, stbrp_rect *rects, int num_rects) {
    int i;
    for (i = 0; i < num_rects; i++) {
        if (con->x + rects[i].w > con->width) {
            con->x = 0;
            con->y = con->bottom_y;
        }
        if (con->y + rects[i].h > con->height) break;
        rects[i].x = con->x;
        rects[i].y = con->y;
        rects[i].was_packed = 1;
        con->x += rects[i].w;
        if (con->y + rects[i].h > con->bottom_y)
            con->bottom_y = con->y + rects[i].h;
    }
    for (; i < num_rects; i++)
        rects[i].was_packed = 0;
}

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

static unsigned char *font_data = NULL;
static int font_data_size = 0;
static stbtt_fontinfo font_info;
bool gcore_font_loaded = false;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

uint32_t gcore_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (a << 24) | (r << 16) | (g << 8) | b;
}

bool gcore_init_font(const char *font_path, int default_size) {
    (void)default_size;
    FILE *f = NULL;
    if (font_path) f = fopen(font_path, "rb");
    if (!f) f = fopen("/usr/share/fonts/TTF/DejaVuSans.ttf", "rb");
    if (!f) f = fopen("/usr/share/fonts/dejavu/DejaVuSans.ttf", "rb");
    if (!f) f = fopen("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "rb");
    if (!f) f = fopen("/usr/share/fonts/truetype/DejaVuSans.ttf", "rb");
    if (!f) f = fopen("/usr/share/fonts/liberation/LiberationSans-Regular.ttf", "rb");
    if (!f) f = fopen("/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf", "rb");
    if (!f) f = fopen("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttf", "rb");
    if (!f) f = fopen("/usr/share/fonts/noto/NotoSansCJK-Regular.ttc", "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    font_data_size = ftell(f);
    rewind(f);
    font_data = malloc(font_data_size);
    fread(font_data, 1, font_data_size, f);
    fclose(f);
    if (!stbtt_InitFont(&font_info, font_data, 0)) {
        free(font_data); font_data = NULL;
        return false;
    }
    gcore_font_loaded = true;
    return true;
}

void gcore_shutdown_font(void) {
    free(font_data); font_data = NULL;
    gcore_font_loaded = false;
}

void draw_text_pixel(PixelBuffer *pb, int x, int y, const char *text, int size, uint32_t color) {
    if (!gcore_font_loaded || !text || size <= 0) return;
    static Shaper *shaper = NULL;
    static int shaper_size = 0;
    if (!shaper || shaper_size != size) {
        if (shaper) shaper_destroy(shaper);
        else shaper = malloc(sizeof(Shaper));
        shaper_init(shaper, font_data, font_data_size, (float)size);
        shaper_size = size;
    }
    ShapedText *st = shaper_shape(shaper, text, 0.0f);
    if (!st || st->count == 0) {
        shaped_text_free(st);
        return;
    }
    float scale = stbtt_ScaleForPixelHeight(&font_info, size);
    int ascent;
    stbtt_GetFontVMetrics(&font_info, &ascent, NULL, NULL);
    int baseline = (int)(ascent * scale);

    for (int i = 0; i < st->count; i++) {
        ShapedGlyph *sg = &st->glyphs[i];
        uint8_t *bitmap;
        int gw, gh, gx, gy;
        if (!shaper_glyph_bitmap(shaper, sg->glyph_index, &bitmap, &gw, &gh, &gx, &gy)) {
            continue;
        }
        if (bitmap) {
            int px = x + (int)sg->x_offset + gx;
            int py = y + baseline + gy;
            for (int row = 0; row < gh; row++) {
                for (int col = 0; col < gw; col++) {
                    uint8_t alpha = bitmap[row * gw + col];
                    if (alpha) {
                        int sx = px + col, sy = py + row;
                        if (sx >= 0 && sx < pb->width && sy >= 0 && sy < pb->height) {
                            uint32_t *pixel = pb->pixels + sy * pb->width + sx;
                            uint32_t bg = *pixel;
                            uint8_t br = (bg >> 16) & 0xFF, bg2 = (bg >> 8) & 0xFF, bb = bg & 0xFF;
                            uint8_t fr = (color >> 16) & 0xFF, fg = (color >> 8) & 0xFF, fb = color & 0xFF;
                            uint8_t r = (fr * alpha + br * (255 - alpha)) / 255;
                            uint8_t g = (fg * alpha + bg2 * (255 - alpha)) / 255;
                            uint8_t b = (fb * alpha + bb * (255 - alpha)) / 255;
                            *pixel = gcore_rgba(r, g, b, 255);
                        }
                    }
                }
            }
            free(bitmap);
        }
    }
    shaped_text_free(st);
}

static void draw_rect(PixelBuffer *pb, int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > pb->width) w = pb->width - x;
    if (y + h > pb->height) h = pb->height - y;
    if (w <= 0 || h <= 0) return;
    for (int row = 0; row < h; row++) {
        uint32_t *line = pb->pixels + (y + row) * pb->width + x;
        for (int col = 0; col < w; col++) line[col] = color;
    }
}

void gcore_draw_rect(PixelBuffer *pb, int x, int y, int w, int h, uint32_t color) {
    draw_rect(pb, x, y, w, h, color);
}

static void draw_rect_alpha(PixelBuffer *pb, int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > pb->width) w = pb->width - x;
    if (y + h > pb->height) h = pb->height - y;
    if (w <= 0 || h <= 0) return;
    uint8_t sa = (color >> 24) & 0xFF;
    if (sa == 0) return;
    if (sa == 255) { draw_rect(pb, x, y, w, h, color); return; }
    uint8_t sr = (color >> 16) & 0xFF, sg = (color >> 8) & 0xFF, sb = color & 0xFF;
    for (int row = 0; row < h; row++) {
        uint32_t *line = pb->pixels + (y + row) * pb->width + x;
        for (int col = 0; col < w; col++) {
            uint32_t bg = line[col];
            uint8_t br = (bg >> 16) & 0xFF, bg2 = (bg >> 8) & 0xFF, bb = bg & 0xFF;
            uint8_t r = (sr * sa + br * (255 - sa)) / 255;
            uint8_t g = (sg * sa + bg2 * (255 - sa)) / 255;
            uint8_t b = (sb * sa + bb * (255 - sa)) / 255;
            line[col] = gcore_rgba(r, g, b, 255);
        }
    }
}

static void draw_rounded_rect(PixelBuffer *pb, int x, int y, int w, int h, int radius, uint32_t color) {
    if (radius <= 0 || radius * 2 > w || radius * 2 > h) {
        draw_rect(pb, x, y, w, h, color);
        return;
    }
    draw_rect(pb, x + radius, y, w - radius * 2, h, color);
    draw_rect(pb, x, y + radius, w, h - radius * 2, color);
    for (int row = 0; row < radius; row++) {
        int offset = radius - (int)sqrtf(radius * radius - (radius - row) * (radius - row));
        uint32_t *top = pb->pixels + (y + row) * pb->width + x + offset;
        uint32_t *bottom = pb->pixels + (y + h - 1 - row) * pb->width + x + offset;
        for (int col = 0; col < w - offset * 2; col++) {
            top[col] = color;
            bottom[col] = color;
        }
    }
}

static void draw_shadow(PixelBuffer *pb, int x, int y, int w, int h, ShadowLayer *sh) {
    if (sh->blur <= 0 && sh->spread <= 0) return;
    int sx = x + sh->offset_x - sh->blur - sh->spread;
    int sy = y + sh->offset_y - sh->blur - sh->spread;
    int sw = w + (sh->blur + sh->spread) * 2;
    int sh_h = h + (sh->blur + sh->spread) * 2;
    uint8_t sa = (sh->color >> 24) & 0xFF;
    for (int row = 0; row < sh_h; row++) {
        for (int col = 0; col < sw; col++) {
            int px = sx + col, py = sy + row;
            if (px < 0 || px >= pb->width || py < 0 || py >= pb->height) continue;
            int dx = col - sh->blur - sh->spread;
            int dy = row - sh->blur - sh->spread;
            float dist = 0.0f;
            if (dx < 0) dist += dx * dx;
            else if (dx >= w) dist += (dx - w + 1) * (dx - w + 1);
            if (dy < 0) dist += dy * dy;
            else if (dy >= sh_h - (sh->blur + sh->spread) * 2) dist += (dy - h + 1) * (dy - h + 1);
            dist = sqrtf(dist);
            float alpha = 1.0f;
            if (sh->blur > 0) alpha = 1.0f - fminf(dist / sh->blur, 1.0f);
            if (alpha <= 0.0f) continue;
            uint8_t a = (uint8_t)(sa * alpha);
            uint32_t *pixel = pb->pixels + py * pb->width + px;
            uint32_t bg = *pixel;
            uint8_t br = (bg >> 16) & 0xFF, bg2 = (bg >> 8) & 0xFF, bb = bg & 0xFF;
            uint8_t sr = (sh->color >> 16) & 0xFF, sg = (sh->color >> 8) & 0xFF, sb = sh->color & 0xFF;
            uint8_t r = (sr * a + br * (255 - a)) / 255;
            uint8_t g = (sg * a + bg2 * (255 - a)) / 255;
            uint8_t b = (sb * a + bb * (255 - a)) / 255;
            *pixel = gcore_rgba(r, g, b, 255);
        }
    }
}

static void draw_gradient_bg(PixelBuffer *pb, int x, int y, int w, int h, Gradient *grad) {
    if (grad->type == GRADIENT_LINEAR) {
        float angle_rad = grad->angle * M_PI / 180.0f;
        float dx = cosf(angle_rad), dy = -sinf(angle_rad);
        for (int row = 0; row < h; row++) {
            uint32_t *line = pb->pixels + (y + row) * pb->width + x;
            for (int col = 0; col < w; col++) {
                float t = ((col / (float)(w > 1 ? w - 1 : 1)) * dx + (row / (float)(h > 1 ? h - 1 : 1)) * dy);
                t = (t + 1.0f) / 2.0f;
                if (t < 0.0f) t = 0.0f;
                if (t > 1.0f) t = 1.0f;
                uint32_t c = grad->stops[0].color;
                for (int s = 1; s < grad->stop_count; s++) {
                    if (t <= grad->stops[s].offset) {
                        float lt = (t - grad->stops[s-1].offset) / (grad->stops[s].offset - grad->stops[s-1].offset);
                        uint32_t a = grad->stops[s-1].color, b = grad->stops[s].color;
                        int ar = (a>>16)&0xFF, ag = (a>>8)&0xFF, ab = a&0xFF;
                        int br = (b>>16)&0xFF, bg = (b>>8)&0xFF, bb = b&0xFF;
                        int r = ar + (int)((br - ar) * lt);
                        int g = ag + (int)((bg - ag) * lt);
                        int bl = ab + (int)((bb - ab) * lt);
                        c = gcore_rgba(r, g, bl, 255);
                        break;
                    }
                }
                line[col] = c;
            }
        }
    } else if (grad->type == GRADIENT_RADIAL) {
        float cx = x + w * grad->center_x, cy = y + h * grad->center_y;
        float max_r = sqrtf(w*w + h*h);
        for (int row = 0; row < h; row++) {
            uint32_t *line = pb->pixels + (y + row) * pb->width + x;
            for (int col = 0; col < w; col++) {
                float dx = (x + col) - cx, dy = (y + row) - cy;
                float t = sqrtf(dx*dx + dy*dy) / max_r;
                if (t < 0.0f) t = 0.0f;
                if (t > 1.0f) t = 1.0f;
                uint32_t c = grad->stops[0].color;
                for (int s = 1; s < grad->stop_count; s++) {
                    if (t <= grad->stops[s].offset) {
                        float lt = (t - grad->stops[s-1].offset) / (grad->stops[s].offset - grad->stops[s-1].offset);
                        uint32_t a = grad->stops[s-1].color, b = grad->stops[s].color;
                        int ar = (a>>16)&0xFF, ag = (a>>8)&0xFF, ab = a&0xFF;
                        int br = (b>>16)&0xFF, bg = (b>>8)&0xFF, bb = b&0xFF;
                        int r = ar + (int)((br - ar) * lt);
                        int g = ag + (int)((bg - ag) * lt);
                        int bl = ab + (int)((bb - ab) * lt);
                        c = gcore_rgba(r, g, bl, 255);
                        break;
                    }
                }
                line[col] = c;
            }
        }
    }
}

static void draw_dashed_line(PixelBuffer *pb, int x, int y, int length, bool horizontal, uint32_t color) {
    int dash = 4, gap = 3;
    int pos = 0;
    while (pos < length) {
        int end = pos + dash;
        if (end > length) end = length;
        for (int i = pos; i < end; i++) {
            if (horizontal) {
                if (x+i >= 0 && x+i < pb->width && y >= 0 && y < pb->height)
                    pb->pixels[y * pb->width + x + i] = color;
            } else {
                if (x >= 0 && x < pb->width && y+i >= 0 && y+i < pb->height)
                    pb->pixels[(y + i) * pb->width + x] = color;
            }
        }
        pos = end + gap;
    }
}

static void draw_border_per_side(PixelBuffer *pb, int x, int y, int w, int h, WidgetStyle *ws) {
    if (ws->border_top_width > 0 && ws->border_top_style == BORDER_SOLID)
        draw_rect(pb, x, y, w, ws->border_top_width, ws->border_top_color);
    else if (ws->border_top_width > 0 && ws->border_top_style == BORDER_DASHED)
        draw_dashed_line(pb, x, y, w, true, ws->border_top_color);

    if (ws->border_bottom_width > 0 && ws->border_bottom_style == BORDER_SOLID)
        draw_rect(pb, x, y + h - ws->border_bottom_width, w, ws->border_bottom_width, ws->border_bottom_color);
    else if (ws->border_bottom_width > 0 && ws->border_bottom_style == BORDER_DASHED)
        draw_dashed_line(pb, x, y + h - ws->border_bottom_width, w, true, ws->border_bottom_color);

    if (ws->border_left_width > 0 && ws->border_left_style == BORDER_SOLID)
        draw_rect(pb, x, y, ws->border_left_width, h, ws->border_left_color);
    else if (ws->border_left_width > 0 && ws->border_left_style == BORDER_DASHED)
        draw_dashed_line(pb, x, y, h, false, ws->border_left_color);

    if (ws->border_right_width > 0 && ws->border_right_style == BORDER_SOLID)
        draw_rect(pb, x + w - ws->border_right_width, y, ws->border_right_width, h, ws->border_right_color);
    else if (ws->border_right_width > 0 && ws->border_right_style == BORDER_DASHED)
        draw_dashed_line(pb, x + w - ws->border_right_width, y, h, false, ws->border_right_color);
}

static void update_hover_states(RenderTree *node, int mx, int my, int off_x, int off_y) {
    if (!node) return;
    int x = off_x + node->rect.x, y = off_y + node->rect.y;
    bool inside = (mx >= x && mx < x + node->rect.w && my >= y && my < y + node->rect.h);
    if (inside) {
        if (!node->state || strcmp(node->state, "hover") != 0) {
            if (node->state && node->state_owned) free(node->state);
            node->state = strdup("hover");
            node->state_owned = true;
        }
    } else {
        if (node->state && node->state_owned) {
            free(node->state);
            node->state = NULL;
            node->state_owned = false;
        }
    }
    if (node->type == RNODE_CONTAINER && node->u.container.children) {
        for (int i = 0; i < node->u.container.child_count; i++)
            update_hover_states(&node->u.container.children[i], mx, my, x, y);
    }
    if (node->type == RNODE_FLEX && node->u.flex.children) {
        for (int i = 0; i < node->u.flex.child_count; i++)
            update_hover_states(&node->u.flex.children[i], mx, my, x, y);
    }
    if (node->type == RNODE_GRID && node->u.grid.children) {
        for (int i = 0; i < node->u.grid.child_count; i++)
            update_hover_states(&node->u.grid.children[i], mx, my, x, y);
    }
}

static void render_node(RenderTree *node, int off_x, int off_y, PixelBuffer *pb, Theme *theme, Arena *arena, bool parent_dirty, Rect dirty_rect) {
    if (!node || node->rect.w <= 0 || node->rect.h <= 0) return;
    if (!node->dirty && !parent_dirty) return;
    int x = off_x + node->rect.x, y = off_y + node->rect.y;
    int w = node->rect.w, h = node->rect.h;
    if (dirty_rect.w > 0 && dirty_rect.h > 0) {
        if (x + w <= dirty_rect.x || x >= dirty_rect.x + dirty_rect.w ||
            y + h <= dirty_rect.y || y >= dirty_rect.y + dirty_rect.h) return;
    }
    bool self_dirty = node->dirty;
    node->dirty = false;
    WidgetStyle *ws = &node->resolved_style;

    for (int i = 0; i < ws->shadow_count; i++)
        draw_shadow(pb, x, y, w, h, &ws->shadows[i]);

    if (ws->gradient.type != GRADIENT_NONE)
        draw_gradient_bg(pb, x, y, w, h, &ws->gradient);
    else
        draw_rounded_rect(pb, x, y, w, h, ws->border_radius, ws->bg_color);

    if (ws->opacity < 1.0f) {
        draw_rect_alpha(pb, x, y, w, h, gcore_rgba(255, 255, 255, (uint8_t)((1.0f - ws->opacity) * 255)));
    }

    draw_border_per_side(pb, x, y, w, h, ws);

    int pad_left = 0, pad_top = 0, pad_right = 0, pad_bottom = 0;
    if (node->type == RNODE_CONTAINER) {
        pad_left = node->u.container.padding.left;
        pad_top = node->u.container.padding.top;
        pad_right = node->u.container.padding.right;
        pad_bottom = node->u.container.padding.bottom;
    }
    int content_x = x + ws->margin[3] + pad_left + ws->border_width;
    int content_y = y + ws->margin[0] + pad_top + ws->border_width;
    int content_w = w - ws->margin[1] - ws->margin[3] - pad_left - pad_right - ws->border_width * 2;
    int content_h = h - ws->margin[2] - ws->margin[0] - pad_top - pad_bottom - ws->border_width * 2;
    if (content_w <= 0) return;

    switch (node->type) {
    case RNODE_TEXT: {
        draw_text_pixel(pb, content_x, content_y, node->u.text.content, ws->font_size, ws->fg_color);
        break;
    }
    case RNODE_CONTAINER:
        for (int i = 0; i < node->u.container.child_count; i++) {
            RenderTree *child = &node->u.container.children[i];
            render_node(child, content_x, content_y, pb, theme, arena, self_dirty || parent_dirty, dirty_rect);
        }
        break;
    case RNODE_FLEX:
        if (node->u.flex.children) {
            for (int i = 0; i < node->u.flex.child_count; i++)
                render_node(&node->u.flex.children[i], content_x, content_y, pb, theme, arena, self_dirty || parent_dirty, dirty_rect);
        }
        break;
    case RNODE_GRID:
        if (node->u.grid.children) {
            for (int i = 0; i < node->u.grid.child_count; i++)
                render_node(&node->u.grid.children[i], content_x, content_y, pb, theme, arena, self_dirty || parent_dirty, dirty_rect);
        }
        break;
    case RNODE_LIST: {
        int item_h = ws->font_size + ws->padding[0] + ws->padding[2];
        for (int i = 0; i < node->u.list.item_count && i * item_h < content_h; i++) {
            int iy = content_y + i * item_h;
            bool is_sel = (i == node->u.list.selected);
            uint32_t bg = is_sel ? ws->accent_color : ws->bg_color;
            uint32_t fg = is_sel ? 0xFFFFFFFF : ws->fg_color;
            draw_rounded_rect(pb, content_x, iy, content_w, item_h, 0, bg);
            draw_text_pixel(pb, content_x + ws->padding[3], iy + ws->padding[0], node->u.list.items[i].label, ws->font_size, fg);
        }
        break;
    }
    case RNODE_INPUT: {
        int input_h = ws->font_size + ws->padding[0] + ws->padding[2];
        draw_rounded_rect(pb, content_x, content_y, content_w, input_h, ws->border_radius, ws->bg_color);
        draw_rect(pb, content_x, content_y, content_w, 1, ws->border_color);
        draw_rect(pb, content_x, content_y + input_h - 1, content_w, 1, ws->border_color);
        draw_rect(pb, content_x, content_y, 1, input_h, ws->border_color);
        draw_rect(pb, content_x + content_w - 1, content_y, 1, input_h, ws->border_color);
        const char *t = node->u.input.text && strlen(node->u.input.text) ? node->u.input.text : (node->u.input.placeholder ? node->u.input.placeholder : "");
        draw_text_pixel(pb, content_x + ws->padding[3], content_y + ws->padding[0], t, ws->font_size, ws->fg_color);
        if (node->u.input.text && !node->u.input.masked && node->u.input.cursor >= 0) {
            int cx = content_x + ws->padding[3] + node->u.input.cursor * (ws->font_size / 2);
            draw_rect(pb, cx, content_y + ws->padding[0], 2, ws->font_size, ws->fg_color);
        }
        break;
    }
    case RNODE_CHECKBOX: {
        int box = ws->font_size;
        draw_rounded_rect(pb, content_x, content_y + ws->padding[0], box, box, 3, ws->bg_color);
        draw_rect(pb, content_x, content_y + ws->padding[0], box, 1, ws->border_color);
        draw_rect(pb, content_x, content_y + ws->padding[0] + box - 1, box, 1, ws->border_color);
        draw_rect(pb, content_x, content_y + ws->padding[0], 1, box, ws->border_color);
        draw_rect(pb, content_x + box - 1, content_y + ws->padding[0], 1, box, ws->border_color);
        if (node->u.checkbox.checked)
            draw_text_pixel(pb, content_x + 2, content_y + ws->padding[0] - 1, "x", ws->font_size, ws->accent_color);
        draw_text_pixel(pb, content_x + box + 6, content_y + ws->padding[0], node->u.checkbox.label, ws->font_size, ws->fg_color);
        break;
    }
    case RNODE_TOGGLE: {
        int toggle_w = ws->font_size * 2 + 4, toggle_h = ws->font_size + 4, knob = ws->font_size;
        draw_rounded_rect(pb, content_x, content_y + ws->padding[0], toggle_w, toggle_h, toggle_h/2, node->u.toggle.value ? ws->accent_color : ws->border_color);
        int kx = node->u.toggle.value ? content_x + toggle_w - knob - 2 : content_x + 2;
        draw_rounded_rect(pb, kx, content_y + ws->padding[0] + 2, knob, knob, knob/2, 0xFFFFFFFF);
        draw_text_pixel(pb, content_x + toggle_w + 8, content_y + ws->padding[0], node->u.toggle.label, ws->font_size, ws->fg_color);
        break;
    }
    case RNODE_SPINNER: {
        const char *frames[] = {"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"};
        draw_text_pixel(pb, content_x, content_y, frames[node->u.spinner.frame % 10], ws->font_size, ws->accent_color);
        draw_text_pixel(pb, content_x + ws->font_size + 8, content_y, node->u.spinner.message, ws->font_size, ws->fg_color);
        break;
    }
    case RNODE_SEPARATOR:
        if (node->u.separator.orientation == ORIENT_HORIZONTAL)
            draw_rect(pb, content_x, content_y + (content_h/2), content_w, 1, ws->border_color);
        else
            draw_rect(pb, content_x + (content_w/2), content_y, 1, content_h, ws->border_color);
        break;
    case RNODE_BADGE:
        draw_rounded_rect(pb, x, y, w, h, 999, ws->accent_color);
        draw_text_pixel(pb, x + ws->padding[3], y + ws->padding[0], node->u.badge.text, ws->font_size, 0xFFFFFFFF);
        break;
    case RNODE_CURSOR:
        draw_rect(pb, content_x + node->u.cursor.x, content_y + node->u.cursor.y, ws->font_size/2, ws->font_size, ws->fg_color);
        break;
    case RNODE_TABLE: {
        int col_w = content_w / (node->u.table.header_count ? node->u.table.header_count : 1);
        int row_h = ws->font_size + ws->padding[0] + ws->padding[2];
        for (int c = 0; c < node->u.table.header_count; c++)
            draw_text_pixel(pb, content_x + c * col_w + ws->padding[3], content_y, node->u.table.headers[c], ws->font_size, ws->accent_color);
        for (int r = 0; r < node->u.table.row_count && (r+1)*row_h < content_h; r++) {
            int ry = content_y + (r+1) * row_h;
            bool sel = (r == node->u.table.selected_row);
            if (sel) draw_rounded_rect(pb, content_x, ry, content_w, row_h, 0, ws->accent_color);
            for (int c = 0; c < node->u.table.header_count; c++)
                draw_text_pixel(pb, content_x + c * col_w + ws->padding[3], ry + ws->padding[0], node->u.table.rows[r][c], ws->font_size, sel ? 0xFFFFFFFF : ws->fg_color);
        }
        break;
    }
    case RNODE_TREE: {
        int row_h = ws->font_size + ws->padding[0] + ws->padding[2];
        for (int i = 0; i < node->u.tree.node_count && i * row_h < content_h; i++) {
            int iy = content_y + i * row_h;
            draw_text_pixel(pb, content_x, iy + ws->padding[0], node->u.tree.nodes[i].expanded ? "▼ " : "▶ ", ws->font_size, ws->fg_color);
            draw_text_pixel(pb, content_x + 16, iy + ws->padding[0], node->u.tree.nodes[i].label, ws->font_size, ws->fg_color);
        }
        break;
    }
    case RNODE_GAUGE: {
        int bar_h = ws->font_size + 4;
        draw_rounded_rect(pb, content_x, content_y + content_h/2 - bar_h/2, content_w, bar_h, bar_h/2, ws->bg_color);
        int fill_w = (content_w - 2) * node->u.gauge.percent / 100;
        if (fill_w > 0) draw_rounded_rect(pb, content_x+1, content_y + content_h/2 - bar_h/2 + 1, fill_w, bar_h-2, bar_h/2-1, ws->accent_color);
        char pct[16]; snprintf(pct, sizeof(pct), "%s %d%%", node->u.gauge.label, node->u.gauge.percent);
        draw_text_pixel(pb, content_x + (content_w - (int)strlen(pct) * ws->font_size/2)/2, content_y + content_h/2 - ws->font_size/2, pct, ws->font_size, 0xFFFFFFFF);
        break;
    }
    case RNODE_CALENDAR: {
        const char *days[] = {"Su","Mo","Tu","We","Th","Fr","Sa"};
        int dim = 31;
        switch (node->u.calendar.month) {
            case 4: case 6: case 9: case 11: dim = 30; break;
            case 2: dim = (node->u.calendar.year % 4 == 0 && (node->u.calendar.year % 100 != 0 || node->u.calendar.year % 400 == 0)) ? 29 : 28; break;
        }
        struct tm tm = { .tm_mday = 1, .tm_mon = node->u.calendar.month - 1, .tm_year = node->u.calendar.year - 1900 };
        mktime(&tm);
        int first_wday = tm.tm_wday;
        int cell_w = (content_w - ws->padding[3] - ws->padding[1]) / 7;
        int cell_h = ws->font_size + 4;
        for (int i = 0; i < 7; i++) {
            int dx = content_x + ws->padding[3] + i * cell_w + (cell_w - 20) / 2;
            draw_text_pixel(pb, dx, content_y + ws->padding[0], days[i], ws->font_size - 2, ws->accent_color);
        }
        int row = 1;
        for (int d = 1; d <= dim; d++) {
            int col = (first_wday + d - 1) % 7;
            if (col == 0 && d > 1) row++;
            char buf[4]; snprintf(buf, sizeof(buf), "%d", d);
            int text_w = (int)strlen(buf) * ws->font_size / 2;
            int dx = content_x + ws->padding[3] + col * cell_w + (cell_w - text_w) / 2;
            int dy_bg = content_y + ws->padding[0] + row * cell_h;
            if (d == node->u.calendar.selected_day) {
                int highlight_w = text_w + 12;
                int hx = content_x + ws->padding[3] + col * cell_w + (cell_w - highlight_w) / 2;
                draw_rounded_rect(pb, hx, dy_bg, highlight_w, cell_h, 4, ws->accent_color);
                draw_text_pixel(pb, dx, dy_bg, buf, ws->font_size, 0xFFFFFFFF);
            } else {
                draw_text_pixel(pb, dx, dy_bg, buf, ws->font_size, ws->fg_color);
            }
        }
        break;
    }
    case RNODE_FORM: {
        int row_h = ws->font_size + ws->padding[0] + ws->padding[2];
        for (int i = 0; i < node->u.form.field_count && i * row_h < content_h; i++) {
            int iy = content_y + i * row_h;
            char buf[256];
            snprintf(buf, sizeof(buf), "%s: %s", node->u.form.fields[i].label, node->u.form.fields[i].value);
            draw_text_pixel(pb, content_x, iy + ws->padding[0], buf, ws->font_size, i == node->u.form.focused ? ws->accent_color : ws->fg_color);
        }
        if (node->u.form.field_count * row_h < content_h) {
            int by = content_y + node->u.form.field_count * row_h;
            draw_rounded_rect(pb, content_x, by, content_w, row_h, ws->border_radius, ws->accent_color);
            draw_text_pixel(pb, content_x + ws->padding[3], by + ws->padding[0], node->u.form.submit_label, ws->font_size, 0xFFFFFFFF);
        }
        break;
    }
    case RNODE_TABS: {
        int tab_h = ws->font_size + ws->padding[0] + ws->padding[2], lx = content_x;
        for (int i = 0; i < node->u.tabs.tab_count; i++) {
            int tw = strlen(node->u.tabs.tab_labels[i]) * (ws->font_size/2) + ws->padding[1] + ws->padding[3];
            if (i == node->u.tabs.active) {
                draw_rounded_rect(pb, lx, content_y, tw, tab_h, ws->border_radius, ws->accent_color);
                draw_text_pixel(pb, lx + ws->padding[3], content_y + ws->padding[0], node->u.tabs.tab_labels[i], ws->font_size, 0xFFFFFFFF);
            } else {
                draw_text_pixel(pb, lx + ws->padding[3], content_y + ws->padding[0], node->u.tabs.tab_labels[i], ws->font_size, ws->fg_color);
            }
            lx += tw + 4;
        }
        if (node->u.tabs.child && tab_h < content_h)
            render_node(node->u.tabs.child, content_x, content_y + tab_h, pb, theme, arena, self_dirty || parent_dirty, dirty_rect);
        break;
    }
    case RNODE_SPLIT_PANES: {
        int sp = node->u.split_panes.split_position > 0 ? node->u.split_panes.split_position :
                  (node->u.split_panes.orientation == ORIENT_HORIZONTAL ? content_w/2 : content_h/2);
        if (node->u.split_panes.orientation == ORIENT_HORIZONTAL) {
            if (node->u.split_panes.first) render_node(node->u.split_panes.first, content_x, content_y, pb, theme, arena, self_dirty || parent_dirty, dirty_rect);
            draw_rect(pb, content_x + sp, content_y, 1, content_h, ws->border_color);
            if (node->u.split_panes.second) render_node(node->u.split_panes.second, content_x + sp + 1, content_y, pb, theme, arena, self_dirty || parent_dirty, dirty_rect);
        } else {
            if (node->u.split_panes.first) render_node(node->u.split_panes.first, content_x, content_y, pb, theme, arena, self_dirty || parent_dirty, dirty_rect);
            draw_rect(pb, content_x, content_y + sp, content_w, 1, ws->border_color);
            if (node->u.split_panes.second) render_node(node->u.split_panes.second, content_x, content_y + sp + 1, pb, theme, arena, self_dirty || parent_dirty, dirty_rect);
        }
        break;
    }
    case RNODE_CONTEXT_MENU: {
        int item_h = ws->font_size + ws->padding[0] + ws->padding[2];
        for (int i = 0; i < node->u.context_menu.item_count && i * item_h < content_h; i++) {
            int iy = content_y + i * item_h;
            if (i == node->u.context_menu.selected)
                draw_rounded_rect(pb, content_x, iy, content_w, item_h, 0, ws->accent_color);
            draw_text_pixel(pb, content_x + ws->padding[3], iy + ws->padding[0], node->u.context_menu.items[i].label, ws->font_size, i == node->u.context_menu.selected ? 0xFFFFFFFF : ws->fg_color);
        }
        break;
    }
    case RNODE_TOAST: {
        int tw = strlen(node->u.toast.message) * (ws->font_size/2) + ws->padding[1] + ws->padding[3];
        int tx = content_x + (content_w - tw)/2;
        int ty = content_y + content_h - ws->font_size - ws->padding[2] - ws->padding[0];
        draw_rounded_rect(pb, tx, ty, tw, ws->font_size + ws->padding[0] + ws->padding[2], ws->border_radius, ws->bg_color);
        draw_text_pixel(pb, tx + ws->padding[3], ty + ws->padding[0], node->u.toast.message, ws->font_size, ws->fg_color);
        break;
    }
    case RNODE_VECTOR: {
        if (node->u.vector.path && node->u.vector.path[0]) {
            VectorPath *vp = vector_parse_svg_path(node->u.vector.path, 1.0f);
            if (vp) {
                int vx, vy, vw, vh;
                vector_get_bounds(vp, &vx, &vy, &vw, &vh, 1.0f);
                float sx = (float)content_w / (float)(vw > 0 ? vw : 1);
                float sy = (float)content_h / (float)(vh > 0 ? vh : 1);
                float sc = sx < sy ? sx : sy;
                int dx = content_x + (content_w - (int)(vw * sc)) / 2;
                int dy = content_y + (content_h - (int)(vh * sc)) / 2;
                vector_rasterize_to_buffer(vp, (uint8_t *)pb->pixels, pb->width, pb->height, pb->width, sc, dx - (int)(vx * sc), dy - (int)(vy * sc));
                vector_path_free(vp);
            }
        }
        break;
    }
    case RNODE_RICH_TEXT: {
        if (node->u.rich_text.spans && node->u.rich_text.spans[0]) {
            draw_text_pixel(pb, content_x, content_y, node->u.rich_text.spans, ws->font_size, ws->fg_color);
        }
        break;
    }
    case RNODE_IMAGE: {
        draw_rounded_rect(pb, content_x, content_y, content_w, content_h, ws->border_radius, ws->accent_color);
        if (node->u.image.source && node->u.image.source[0]) {
            char label[512];
            snprintf(label, sizeof(label), "[Image: %s]", node->u.image.source);
            draw_text_pixel(pb, content_x + ws->padding[3], content_y + ws->padding[0], label, ws->font_size - 2, ws->fg_color);
        }
        break;
    }
    case RNODE_MARKDOWN: {
        if (node->u.markdown.content && node->u.markdown.content[0]) {
            draw_text_pixel(pb, content_x, content_y, node->u.markdown.content, ws->font_size, ws->fg_color);
        }
        break;
    }
    case RNODE_PLOT: {
        draw_rounded_rect(pb, content_x, content_y, content_w, content_h, ws->border_radius, ws->bg_color);
        if (node->u.plot.type && node->u.plot.data_count > 0) {
            float min_val = node->u.plot.data[0], max_val = node->u.plot.data[0];
            for (int i = 1; i < node->u.plot.data_count; i++) {
                if (node->u.plot.data[i] < min_val) min_val = node->u.plot.data[i];
                if (node->u.plot.data[i] > max_val) max_val = node->u.plot.data[i];
            }
            float range = max_val - min_val;
            if (range < 0.001f) range = 1.0f;
            int plot_w = content_w - ws->padding[1] - ws->padding[3];
            int plot_h = content_h - ws->padding[0] - ws->padding[2];
            int px0 = content_x + ws->padding[3], py0 = content_y + ws->padding[0] + plot_h;
            if (strcmp(node->u.plot.type, "bar") == 0) {
                int bar_w = plot_w / node->u.plot.data_count;
                for (int i = 0; i < node->u.plot.data_count; i++) {
                    int bh = (int)((node->u.plot.data[i] - min_val) / range * plot_h);
                    draw_rect(pb, px0 + i * bar_w + 1, py0 - bh, bar_w - 2, bh, ws->accent_color);
                }
            } else {
                for (int i = 0; i < node->u.plot.data_count - 1; i++) {
                    int x1 = px0 + i * plot_w / (node->u.plot.data_count - 1);
                    int y1 = py0 - (int)((node->u.plot.data[i] - min_val) / range * plot_h);
                    int x2 = px0 + (i + 1) * plot_w / (node->u.plot.data_count - 1);
                    int y2 = py0 - (int)((node->u.plot.data[i + 1] - min_val) / range * plot_h);
                    int steps = abs(x2 - x1) + abs(y2 - y1);
                    if (steps < 1) steps = 1;
                    for (int step = 0; step <= steps; step++) {
                        int sx = x1 + (x2 - x1) * step / steps;
                        int sy = y1 + (y2 - y1) * step / steps;
                        if (sx >= 0 && sx < pb->width && sy >= 0 && sy < pb->height)
                            pb->pixels[sy * pb->width + sx] = ws->accent_color;
                    }
                }
            }
            if (node->u.plot.labels && node->u.plot.label_count > 0) {
                for (int i = 0; i < node->u.plot.label_count && i < node->u.plot.data_count; i++) {
                    int lx = px0 + i * plot_w / (node->u.plot.data_count - 1);
                    draw_text_pixel(pb, lx - 8, py0 + 2, node->u.plot.labels[i], ws->font_size - 4, ws->fg_color);
                }
            }
        }
        break;
    }
    case RNODE_CANVAS:
    case RNODE_VIDEO: {
        draw_rounded_rect(pb, content_x, content_y, content_w, content_h, ws->border_radius, ws->bg_color);
        const char *label = node->type == RNODE_CANVAS ? "[Canvas]" : "[Video]";
        draw_text_pixel(pb, content_x + ws->padding[3], content_y + ws->padding[0], label, ws->font_size, ws->fg_color);
        break;
    }
    }

    if (node->tooltip && node->tooltip[0]) {
        extern char *tooltip_target_id;
        extern int tooltip_hover_x, tooltip_hover_y;
        if (tooltip_target_id && node->style_class && strcmp(tooltip_target_id, node->style_class) == 0) {
            int tw = strlen(node->tooltip) * (ws->font_size / 2) + 12;
            int th = ws->font_size + 8;
            int tx = tooltip_hover_x + 12;
            int ty = tooltip_hover_y - th - 4;
            if (tx + tw > pb->width) tx = pb->width - tw - 4;
            if (ty < 0) ty = tooltip_hover_y + 16;
            draw_rounded_rect(pb, tx, ty, tw, th, 4, gcore_rgba(40, 40, 40, 240));
            draw_text_pixel(pb, tx + 6, ty + 4, node->tooltip, ws->font_size - 2, 0xFFFFFFFF);
        }
    }
}

void gcore_render_tree_to_pixels(RenderTree *tree, PixelBuffer *pb, Theme *theme, Arena *arena) {
    if (!tree || !pb || !pb->pixels) return;
    update_hover_states(tree, pb->mouse_x, pb->mouse_y, 0, 0);
    Rect full = {0, 0, 0, 0};
    render_node(tree, 0, 0, pb, theme, arena, true, full);

#ifdef FILLY_PROFILING
    extern double session_current_fps;
    extern Arena *g_session_arena;
    if (session_current_fps > 0 && gcore_font_loaded) {
        char hud[64];
        snprintf(hud, sizeof(hud), "FPS:%.0f Arena:%zuKB",
            session_current_fps,
            g_session_arena ? g_session_arena->offset / 1024 : 0);
        draw_text_pixel(pb, pb->width - 200, 5, hud, 10, 0xFF00FF00);
    }
#endif
}