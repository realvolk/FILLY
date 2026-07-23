#include "renderer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

static unsigned char *font_data = NULL;
static int font_data_size = 0;
static stbtt_fontinfo font_info;
bool gcore_font_loaded = false;

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

void draw_text_pixel(PixelBuffer *pb, int x, int y, const char *text, int size, uint32_t color) {
    if (!gcore_font_loaded || !text || size <= 0) return;
    float scale = stbtt_ScaleForPixelHeight(&font_info, size);
    int ascent;
    stbtt_GetFontVMetrics(&font_info, &ascent, 0, 0);
    int baseline = (int)(ascent * scale);
    int pen_x = x;
    while (*text) {
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font_info, *text, &advance, &lsb);
        int x0 = pen_x + (int)(lsb * scale);
        int y0 = y + baseline;
        int cw, ch;
        unsigned char *bitmap = stbtt_GetCodepointBitmap(&font_info, scale, scale, *text, &cw, &ch, NULL, NULL);
        if (bitmap) {
            for (int row = 0; row < ch; row++) {
                for (int col = 0; col < cw; col++) {
                    uint8_t alpha = bitmap[row * cw + col];
                    if (alpha) {
                        int px = x0 + col, py = y0 + row;
                        if (px >= 0 && px < pb->width && py >= 0 && py < pb->height) {
                            uint32_t *pixel = pb->pixels + py * pb->width + px;
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
            stbtt_FreeBitmap(bitmap, NULL);
        }
        pen_x += (int)(advance * scale);
        text++;
    }
}

static void draw_border(PixelBuffer *pb, int x, int y, int w, int h, int width, int radius, uint32_t color) {
    if (width <= 0) return;
    if (radius > 0) {
        for (int i = 0; i < width; i++) {
            draw_rect(pb, x + radius, y + i, w - radius * 2, 1, color);
            draw_rect(pb, x + radius, y + h - 1 - i, w - radius * 2, 1, color);
            draw_rect(pb, x + i, y + radius, 1, h - radius * 2, color);
            draw_rect(pb, x + w - 1 - i, y + radius, 1, h - radius * 2, color);
        }
    } else {
        draw_rect(pb, x, y, w, width, color);
        draw_rect(pb, x, y + h - width, w, width, color);
        draw_rect(pb, x, y, width, h, color);
        draw_rect(pb, x + w - width, y, width, h, color);
    }
}

static void update_hover_states(RenderTree *node, int mx, int my, int off_x, int off_y) {
    if (!node) return;
    int x = off_x + node->rect.x, y = off_y + node->rect.y;
    bool inside = (mx >= x && mx < x + node->rect.w && my >= y && my < y + node->rect.h);
    if (inside) {
        if (!node->state || strcmp(node->state, "hover") != 0) {
            if (node->state) free(node->state);
            node->state = strdup("hover");
        }
    } else {
        if (node->state) { free(node->state); node->state = NULL; }
    }
    if (node->type == RNODE_CONTAINER && node->container.children) {
        WidgetStyle *s = &node->resolved_style;
        int pad_left = node->container.padding.left;
        int pad_top = node->container.padding.top;
        int cx = x + s->margin[3] + pad_left + s->border_width;
        int cy = y + s->margin[0] + pad_top + s->border_width;
        for (int i = 0; i < node->container.child_count; i++)
            update_hover_states(&node->container.children[i], mx, my, cx, cy);
    }
}

static void render_node(RenderTree *node, int off_x, int off_y, PixelBuffer *pb, Theme *theme, Arena *arena, bool parent_dirty) {
    if (!node || node->rect.w <= 0 || node->rect.h <= 0) return;
    if (!node->dirty && !parent_dirty) return;
    bool self_dirty = node->dirty;
    node->dirty = false;
    int x = off_x + node->rect.x, y = off_y + node->rect.y;
    int w = node->rect.w, h = node->rect.h;
    WidgetStyle *s = &node->resolved_style;

    draw_rounded_rect(pb, x, y, w, h, s->border_radius, s->bg_color);
    if (s->border_width > 0 && node->type == RNODE_CONTAINER)
        draw_border(pb, x, y, w, h, s->border_width, 0, s->border_color);

    int pad_left = 0, pad_top = 0, pad_right = 0, pad_bottom = 0;
    if (node->type == RNODE_CONTAINER) {
        pad_left = node->container.padding.left;
        pad_top = node->container.padding.top;
        pad_right = node->container.padding.right;
        pad_bottom = node->container.padding.bottom;
    }
    int content_x = x + s->margin[3] + pad_left + s->border_width;
    int content_y = y + s->margin[0] + pad_top + s->border_width;
    int content_w = w - s->margin[1] - s->margin[3] - pad_left - pad_right - s->border_width * 2;
    int content_h = h - s->margin[2] - s->margin[0] - pad_top - pad_bottom - s->border_width * 2;
    if (content_w <= 0 || content_h <= 0) return;

    switch (node->type) {
    case RNODE_TEXT:
        draw_text_pixel(pb, content_x, content_y, node->text.content, s->font_size, s->fg_color);
        break;
    case RNODE_CONTAINER:
        for (int i = 0; i < node->container.child_count; i++)
            render_node(&node->container.children[i], content_x, content_y, pb, theme, arena, self_dirty || parent_dirty);
        break;
    case RNODE_LIST: {
        int item_h = s->font_size + s->padding[0] + s->padding[2];
        for (int i = 0; i < node->list.item_count && i * item_h < content_h; i++) {
            int iy = content_y + i * item_h;
            bool is_sel = (i == node->list.selected);
            uint32_t bg = is_sel ? s->accent_color : s->bg_color;
            uint32_t fg = is_sel ? 0xFFFFFFFF : s->fg_color;
            draw_rounded_rect(pb, content_x, iy, content_w, item_h, 0, bg);
            draw_text_pixel(pb, content_x + s->padding[3], iy + s->padding[0], node->list.items[i].label, s->font_size, fg);
        }
        break;
    }
    case RNODE_INPUT: {
        int input_h = s->font_size + s->padding[0] + s->padding[2];
        draw_rounded_rect(pb, content_x, content_y, content_w, input_h, s->border_radius, s->bg_color);
        draw_border(pb, content_x, content_y, content_w, input_h, 1, s->border_radius, s->border_color);
        const char *t = node->input.text && strlen(node->input.text) ? node->input.text : (node->input.placeholder ? node->input.placeholder : "");
        draw_text_pixel(pb, content_x + s->padding[3], content_y + s->padding[0], t, s->font_size, s->fg_color);
        if (node->input.text && !node->input.masked && node->input.cursor >= 0) {
            int cx = content_x + s->padding[3] + node->input.cursor * (s->font_size / 2);
            draw_rect(pb, cx, content_y + s->padding[0], 2, s->font_size, s->fg_color);
        }
        break;
    }
    case RNODE_CHECKBOX: {
        int box = s->font_size;
        draw_rounded_rect(pb, content_x, content_y + s->padding[0], box, box, 3, s->bg_color);
        draw_border(pb, content_x, content_y + s->padding[0], box, box, 1, 3, s->border_color);
        if (node->checkbox.checked)
            draw_text_pixel(pb, content_x + 2, content_y + s->padding[0] - 1, "x", s->font_size, s->accent_color);
        draw_text_pixel(pb, content_x + box + 6, content_y + s->padding[0], node->checkbox.label, s->font_size, s->fg_color);
        break;
    }
    case RNODE_TOGGLE: {
        int toggle_w = s->font_size * 2 + 4, toggle_h = s->font_size + 4, knob = s->font_size;
        draw_rounded_rect(pb, content_x, content_y + s->padding[0], toggle_w, toggle_h, toggle_h/2, node->toggle.value ? s->accent_color : s->border_color);
        int kx = node->toggle.value ? content_x + toggle_w - knob - 2 : content_x + 2;
        draw_rounded_rect(pb, kx, content_y + s->padding[0] + 2, knob, knob, knob/2, 0xFFFFFFFF);
        draw_text_pixel(pb, content_x + toggle_w + 8, content_y + s->padding[0], node->toggle.label, s->font_size, s->fg_color);
        break;
    }
    case RNODE_SPINNER: {
        const char *frames[] = {"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"};
        draw_text_pixel(pb, content_x, content_y, frames[node->spinner.frame % 10], s->font_size, s->accent_color);
        draw_text_pixel(pb, content_x + s->font_size + 8, content_y, node->spinner.message, s->font_size, s->fg_color);
        break;
    }
    case RNODE_SEPARATOR:
        if (node->separator.orientation == ORIENT_HORIZONTAL)
            draw_rect(pb, content_x, content_y + (content_h/2), content_w, 1, s->border_color);
        else
            draw_rect(pb, content_x + (content_w/2), content_y, 1, content_h, s->border_color);
        break;
    case RNODE_BADGE:
        draw_rounded_rect(pb, content_x, content_y, content_w, content_h, 999, s->accent_color);
        draw_text_pixel(pb, content_x + s->padding[3], content_y + s->padding[0], node->badge.text, s->font_size, 0xFFFFFFFF);
        break;
    case RNODE_CURSOR:
        draw_rect(pb, content_x + node->cursor.x, content_y + node->cursor.y, s->font_size/2, s->font_size, s->fg_color);
        break;
    case RNODE_TABLE: {
        int col_w = content_w / (node->table.header_count ? node->table.header_count : 1);
        int row_h = s->font_size + s->padding[0] + s->padding[2];
        for (int c = 0; c < node->table.header_count; c++)
            draw_text_pixel(pb, content_x + c * col_w + s->padding[3], content_y, node->table.headers[c], s->font_size, s->accent_color);
        for (int r = 0; r < node->table.row_count && (r+1)*row_h < content_h; r++) {
            int ry = content_y + (r+1) * row_h;
            bool sel = (r == node->table.selected_row);
            if (sel) draw_rounded_rect(pb, content_x, ry, content_w, row_h, 0, s->accent_color);
            for (int c = 0; c < node->table.header_count; c++)
                draw_text_pixel(pb, content_x + c * col_w + s->padding[3], ry + s->padding[0], node->table.rows[r][c], s->font_size, sel ? 0xFFFFFFFF : s->fg_color);
        }
        break;
    }
    case RNODE_TREE: {
        int row_h = s->font_size + s->padding[0] + s->padding[2];
        int flat[256], flat_count = 0;
        for (int i = 0; i < node->tree.node_count; i++) {
            flat[flat_count++] = i;
            if (node->tree.nodes[i].expanded && node->tree.nodes[i].child_count > 0)
                for (int j = 0; j < node->tree.nodes[i].child_count; j++)
                    flat[flat_count++] = node->tree.node_count + j;
        }
        for (int i = 0; i < flat_count && i * row_h < content_h; i++) {
            int idx = flat[i], iy = content_y + i * row_h;
            TreeNode *tn = (idx < node->tree.node_count) ? &node->tree.nodes[idx] : NULL;
            if (tn && tn->child_count > 0)
                draw_text_pixel(pb, content_x, iy + s->padding[0], tn->expanded ? "▼ " : "▶ ", s->font_size, s->fg_color);
            else
                draw_text_pixel(pb, content_x, iy + s->padding[0], "  ", s->font_size, s->fg_color);
            if (tn) draw_text_pixel(pb, content_x + 16, iy + s->padding[0], tn->label, s->font_size, s->fg_color);
        }
        break;
    }
    case RNODE_GAUGE: {
        int bar_h = s->font_size + 4;
        draw_rounded_rect(pb, content_x, content_y + content_h/2 - bar_h/2, content_w, bar_h, bar_h/2, s->bg_color);
        draw_border(pb, content_x, content_y + content_h/2 - bar_h/2, content_w, bar_h, 1, bar_h/2, s->border_color);
        int fill_w = (content_w - 2) * node->gauge.percent / 100;
        if (fill_w > 0) draw_rounded_rect(pb, content_x+1, content_y + content_h/2 - bar_h/2 + 1, fill_w, bar_h-2, bar_h/2-1, s->accent_color);
        char pct[16]; snprintf(pct, sizeof(pct), "%s %d%%", node->gauge.label, node->gauge.percent);
        draw_text_pixel(pb, content_x + (content_w - (int)strlen(pct) * s->font_size/2)/2, content_y + content_h/2 - s->font_size/2, pct, s->font_size, 0xFFFFFFFF);
        break;
    }
    case RNODE_CALENDAR:
        draw_text_pixel(pb, content_x, content_y, "Calendar", s->font_size, s->fg_color);
        break;
    case RNODE_FORM: {
        int row_h = s->font_size + s->padding[0] + s->padding[2];
        for (int i = 0; i < node->form.field_count && i * row_h < content_h; i++) {
            int iy = content_y + i * row_h;
            char buf[256];
            snprintf(buf, sizeof(buf), "%s: %s", node->form.fields[i].label, node->form.fields[i].value);
            draw_text_pixel(pb, content_x, iy + s->padding[0], buf, s->font_size, i == node->form.focused ? s->accent_color : s->fg_color);
        }
        if (node->form.field_count * row_h < content_h) {
            int by = content_y + node->form.field_count * row_h;
            draw_rounded_rect(pb, content_x, by, content_w, row_h, s->border_radius, s->accent_color);
            draw_text_pixel(pb, content_x + s->padding[3], by + s->padding[0], node->form.submit_label, s->font_size, 0xFFFFFFFF);
        }
        break;
    }
    case RNODE_TABS: {
        int tab_h = s->font_size + s->padding[0] + s->padding[2], lx = content_x;
        for (int i = 0; i < node->tabs.tab_count; i++) {
            int tw = strlen(node->tabs.tab_labels[i]) * (s->font_size/2) + s->padding[1] + s->padding[3];
            if (i == node->tabs.active) {
                draw_rounded_rect(pb, lx, content_y, tw, tab_h, s->border_radius, s->accent_color);
                draw_text_pixel(pb, lx + s->padding[3], content_y + s->padding[0], node->tabs.tab_labels[i], s->font_size, 0xFFFFFFFF);
            } else {
                draw_text_pixel(pb, lx + s->padding[3], content_y + s->padding[0], node->tabs.tab_labels[i], s->font_size, s->fg_color);
            }
            lx += tw + 4;
        }
        if (node->tabs.child && tab_h < content_h)
            render_node(node->tabs.child, content_x, content_y + tab_h, pb, theme, arena, self_dirty || parent_dirty);
        break;
    }
    case RNODE_SPLIT_PANES: {
        int sp = node->split_panes.split_position > 0 ? node->split_panes.split_position :
                  (node->split_panes.orientation == ORIENT_HORIZONTAL ? content_w/2 : content_h/2);
        if (node->split_panes.orientation == ORIENT_HORIZONTAL) {
            render_node(node->split_panes.first, content_x, content_y, pb, theme, arena, self_dirty || parent_dirty);
            draw_rect(pb, content_x + sp, content_y, 1, content_h, s->border_color);
            render_node(node->split_panes.second, content_x + sp + 1, content_y, pb, theme, arena, self_dirty || parent_dirty);
        } else {
            render_node(node->split_panes.first, content_x, content_y, pb, theme, arena, self_dirty || parent_dirty);
            draw_rect(pb, content_x, content_y + sp, content_w, 1, s->border_color);
            render_node(node->split_panes.second, content_x, content_y + sp + 1, pb, theme, arena, self_dirty || parent_dirty);
        }
        break;
    }
    case RNODE_CONTEXT_MENU: {
        int item_h = s->font_size + s->padding[0] + s->padding[2];
        for (int i = 0; i < node->context_menu.item_count && i * item_h < content_h; i++) {
            int iy = content_y + i * item_h;
            if (i == node->context_menu.selected)
                draw_rounded_rect(pb, content_x, iy, content_w, item_h, 0, s->accent_color);
            draw_text_pixel(pb, content_x + s->padding[3], iy + s->padding[0], node->context_menu.items[i].label, s->font_size, i == node->context_menu.selected ? 0xFFFFFFFF : s->fg_color);
        }
        break;
    }
    case RNODE_TOAST: {
        int tw = strlen(node->toast.message) * (s->font_size/2) + s->padding[1] + s->padding[3];
        int tx = content_x + (content_w - tw)/2;
        int ty = content_y + content_h - s->font_size - s->padding[2] - s->padding[0];
        draw_rounded_rect(pb, tx, ty, tw, s->font_size + s->padding[0] + s->padding[2], s->border_radius, s->bg_color);
        draw_border(pb, tx, ty, tw, s->font_size + s->padding[0] + s->padding[2], 1, s->border_radius, s->border_color);
        draw_text_pixel(pb, tx + s->padding[3], ty + s->padding[0], node->toast.message, s->font_size, s->fg_color);
        break;
    }
    }
}

void gcore_render_tree_to_pixels(RenderTree *tree, PixelBuffer *pb, Theme *theme, Arena *arena) {
    if (!tree || !pb || !pb->pixels) return;
    update_hover_states(tree, pb->mouse_x, pb->mouse_y, 0, 0);
    render_node(tree, 0, 0, pb, theme, arena, true);
}