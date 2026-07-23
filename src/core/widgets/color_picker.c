#include "color_picker.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { WidgetBase base; char *title; int r, g, b, channel; } ColorPickerData;
extern Arena *g_session_arena;

static void cp_render(Widget *self, Rect area, RenderTree *out) {
    ColorPickerData *d = (ColorPickerData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = 50, box_h = 11;
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_h > area.h - 2) box_h = area.h - 2;
    RenderTree *children = arena_alloc(g_session_arena, 8 * sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = arena_strdup(g_session_arena, d->title);
    children[0].style_class = "text";
    children[0].state = "title";
    children[1].type = RNODE_TEXT;
    children[1].rect = rect_new(1, 1, box_w - 2, 1);
    char hex[16];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", d->r, d->g, d->b);
    children[1].text.content = arena_strdup(g_session_arena, hex);
    children[1].style_class = "text";
    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, 2, box_w - 2, 1);
    int preview_len = box_w - 4;
    if (preview_len > 48) preview_len = 48;
    if (preview_len < 0) preview_len = 0;
    char *preview = arena_alloc(g_session_arena, preview_len + 1);
    memset(preview, '#', preview_len);
    preview[preview_len] = '\0';
    children[2].text.content = preview;
    children[2].style_class = "text";
    const char *channels[] = {"R","G","B"};
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
        children[3 + i].text.content = arena_strdup(g_session_arena, line);
        children[3 + i].style_class = "text";
    }
    children[6].type = RNODE_TEXT;
    children[6].rect = rect_new(1, 8, box_w - 2, 1);
    children[6].text.content = "Up/Down:channel  Left/Right:-/+  Enter:confirm  Esc:cancel";
    children[6].style_class = "text";
    children[6].state = "muted";
    children[7].type = RNODE_TEXT;
    children[7].rect = rect_new(1, 9, box_w - 2, 1);
    children[7].text.content = "";
    out->type = RNODE_CONTAINER;
    out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 8;
}

static EventResult cp_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    ColorPickerData *d = (ColorPickerData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_UP:
            if (d->channel > 0) d->channel--;
            d->base.dirty = true;
            return event_result_handled();
        case KEY_DOWN:
            if (d->channel < 2) d->channel++;
            d->base.dirty = true;
            return event_result_handled();
        case KEY_LEFT: {
            int *v = d->channel == 0 ? &d->r : d->channel == 1 ? &d->g : &d->b;
            *v = *v - 1 >= 0 ? *v - 1 : 0;
            d->base.dirty = true;
            return event_result_handled();
        }
        case KEY_RIGHT: {
            int *v = d->channel == 0 ? &d->r : d->channel == 1 ? &d->g : &d->b;
            *v = *v + 1 <= 255 ? *v + 1 : 255;
            d->base.dirty = true;
            return event_result_handled();
        }
        case KEY_ENTER: {
            char h[8];
            snprintf(h, sizeof(h), "#%02X%02X%02X", d->r, d->g, d->b);
            return event_result_response((WidgetResponse){ .result = cJSON_CreateString(h), .cancelled = false });
        }
        default:
            return event_result_unhandled();
    }
}

static void cp_destroy(Widget *self) {
    free(((ColorPickerData *)(self + 1))->title);
}

Widget *color_picker_widget_new(const char *title, char **colors, int color_count) {
    (void)colors;
    (void)color_count;
    Widget *w = calloc(1, sizeof(Widget) + sizeof(ColorPickerData));
    ColorPickerData *d = (ColorPickerData *)(w + 1);
    d->base.dirty = true;
    d->title = strdup(title);
    d->r = 128;
    d->g = 128;
    d->b = 128;
    d->channel = 0;
    w->vtable.render = cp_render;
    w->vtable.handle_event = cp_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = cp_destroy;
    return w;
}