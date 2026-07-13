#include "color_picker.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char *title;
    int r, g, b;
    int channel;
    bool dirty;
} ColorPickerData;

static int rgb_to_256(int r, int g, int b) {
    if (r == g && g == b) {
        if (r < 8) return 16;
        if (r > 248) return 231;
        return 232 + (r - 8) / 10;
    }
    int ri = (r * 5 + 127) / 255;
    int gi = (g * 5 + 127) / 255;
    int bi = (b * 5 + 127) / 255;
    return 16 + 36 * ri + 6 * gi + bi;
}

static void cp_render(Widget *self, Rect area, RenderTree *out) {
    ColorPickerData *d = (ColorPickerData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = 50, box_h = 11;
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    RenderTree *children = calloc(8, sizeof(RenderTree));

    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = strdup(d->title);
    children[0].text.align = ALIGN_CENTER;
    children[0].text.style = textstyle_selected();

    children[1].type = RNODE_TEXT;
    children[1].rect = rect_new(1, 1, box_w - 2, 1);
    char hex[16];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", d->r, d->g, d->b);
    children[1].text.content = strdup(hex);
    children[1].text.align = ALIGN_CENTER;
    int color_idx = rgb_to_256(d->r, d->g, d->b);
    char fg_buf[16];
    snprintf(fg_buf, sizeof(fg_buf), "%d", color_idx);
    TextStyle hex_style = { .fg = strdup(fg_buf), .bg = NULL, .bold = true };
    children[1].text.style = hex_style;

    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, 2, box_w - 2, 1);
    char preview[64];
    int preview_len = box_w - 4;
    if (preview_len > 48) preview_len = 48;
    if (preview_len < 0) preview_len = 0;
    memset(preview, '#', preview_len);
    preview[preview_len] = '\0';
    children[2].text.content = strdup(preview);
    children[2].text.align = ALIGN_LEFT;
    TextStyle prev_style = { .fg = strdup(fg_buf), .bg = NULL, .bold = true };
    children[2].text.style = prev_style;

    const char *channels[] = {"R", "G", "B"};
    int vals[] = {d->r, d->g, d->b};
    for (int i = 0; i < 3; i++) {
        int y = 4 + i;
        children[3 + i].type = RNODE_TEXT;
        children[3 + i].rect = rect_new(1, y, box_w - 2, 1);
        char line[128];
        int bar_w = box_w - 20;
        int filled = (int)((float)vals[i] / 255 * bar_w);
        char bar[128] = {0};
        for (int j = 0; j < filled; j++) bar[j] = '#';
        for (int j = filled; j < bar_w; j++) bar[j] = '.';
        snprintf(line, sizeof(line), "%s %s %3d %s", i == d->channel ? ">" : " ", channels[i], vals[i], bar);
        children[3 + i].text.content = strdup(line);
        children[3 + i].text.align = ALIGN_LEFT;
        children[3 + i].text.style = textstyle_normal();
    }

    children[6].type = RNODE_TEXT;
    children[6].rect = rect_new(1, 8, box_w - 2, 1);
    children[6].text.content = strdup("Up/Down:channel  Left/Right:-/+  Enter:confirm  Esc:cancel");
    children[6].text.align = ALIGN_CENTER;
    children[6].text.style = textstyle_muted();

    children[7].type = RNODE_TEXT;
    children[7].rect = rect_new(1, 9, box_w - 2, 1);
    children[7].text.content = strdup("");
    children[7].text.align = ALIGN_LEFT;
    children[7].text.style = textstyle_normal();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 8;
}

static EventResult cp_handle_event(Widget *self, Event *ev, Backend *backend) {
    ColorPickerData *d = (ColorPickerData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_UP: if (d->channel > 0) d->channel--; d->dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->channel < 2) d->channel++; d->dirty = true; return event_result_handled();
        case KEY_LEFT: {
            int *v = d->channel == 0 ? &d->r : d->channel == 1 ? &d->g : &d->b;
            *v = *v - 1 >= 0 ? *v - 1 : 0; d->dirty = true;
            return event_result_handled();
        }
        case KEY_RIGHT: {
            int *v = d->channel == 0 ? &d->r : d->channel == 1 ? &d->g : &d->b;
            *v = *v + 1 <= 255 ? *v + 1 : 255; d->dirty = true;
            return event_result_handled();
        }
        case KEY_ENTER: {
            char hex[8];
            snprintf(hex, sizeof(hex), "#%02X%02X%02X", d->r, d->g, d->b);
            return event_result_response((WidgetResponse){ .result = cJSON_CreateString(hex), .cancelled = false, .error = NULL });
        }
        default: return event_result_unhandled();
    }
}

static bool cp_is_dirty(Widget *self) { return ((ColorPickerData *)(self + 1))->dirty; }
static void cp_clear_dirty(Widget *self) { ((ColorPickerData *)(self + 1))->dirty = false; }
static void cp_destroy(Widget *self) { free(((ColorPickerData *)(self + 1))->title); }

Widget *color_picker_widget_new(const char *title, char **colors, int color_count) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(ColorPickerData));
    w->vtable.render = cp_render;
    w->vtable.handle_event = cp_handle_event;
    w->vtable.is_dirty = cp_is_dirty;
    w->vtable.clear_dirty = cp_clear_dirty;
    w->vtable.destroy = cp_destroy;
    ColorPickerData *d = (ColorPickerData *)(w + 1);
    d->title = strdup(title);
    d->r = 128; d->g = 128; d->b = 128; d->channel = 0;
    d->dirty = true;
    return w;
}