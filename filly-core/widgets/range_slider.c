#include "range_slider.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char *title;
    int min;
    int max;
    int value;
    char *label;
    bool dirty;
} RangeSliderData;

static void rs_render(Widget *self, Rect area, RenderTree *out) {
    RangeSliderData *d = (RangeSliderData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = 50, box_h = 7;
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    int range = d->max - d->min;
    int bar_w = box_w - 4;
    if (bar_w < 3) bar_w = 3;
    int pos = range == 0 ? 0 : (int)((float)(d->value - d->min) / range * (bar_w - 1));

    RenderTree *children = calloc(4, sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = strdup(d->title);
    children[0].text.align = ALIGN_CENTER;
    children[0].text.style = textstyle_selected();

    children[1].type = RNODE_TEXT;
    children[1].rect = rect_new(1, 2, box_w - 2, 1);
    char *bar = calloc(bar_w + 1, 1);
    for (int i = 0; i < pos; i++) bar[i] = '-';
    bar[pos] = 'O';
    for (int i = pos + 1; i < bar_w; i++) bar[i] = '-';
    children[1].text.content = bar;
    children[1].text.align = ALIGN_LEFT;
    children[1].text.style = textstyle_accent();

    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, 4, box_w - 2, 1);
    char label[128];
    int pct = range == 0 ? 100 : (int)((float)(d->value - d->min) / range * 100);
    snprintf(label, sizeof(label), "%s: %d (%d%%)", d->label, d->value, pct);
    children[2].text.content = strdup(label);
    children[2].text.align = ALIGN_CENTER;
    children[2].text.style = textstyle_muted();

    children[3].type = RNODE_TEXT;
    children[3].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[3].text.content = strdup("Left/Right:adjust  Enter:confirm  Esc:cancel");
    children[3].text.align = ALIGN_CENTER;
    children[3].text.style = textstyle_muted();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 4;
}

static EventResult rs_handle_event(Widget *self, Event *ev, Backend *backend) {
    RangeSliderData *d = (RangeSliderData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_LEFT: d->value = d->value - 1 >= d->min ? d->value - 1 : d->min; d->dirty = true; return event_result_handled();
        case KEY_RIGHT: d->value = d->value + 1 <= d->max ? d->value + 1 : d->max; d->dirty = true; return event_result_handled();
        case KEY_ENTER:
            return event_result_response((WidgetResponse){ .result = cJSON_CreateNumber(d->value), .cancelled = false, .error = NULL });
        default: return event_result_unhandled();
    }
}

static bool rs_is_dirty(Widget *self) { return ((RangeSliderData *)(self + 1))->dirty; }
static void rs_clear_dirty(Widget *self) { ((RangeSliderData *)(self + 1))->dirty = false; }
static void rs_destroy(Widget *self) {
    RangeSliderData *d = (RangeSliderData *)(self + 1);
    free(d->title); free(d->label);
}

Widget *range_slider_widget_new(const char *title, int min, int max, int value, const char *label) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(RangeSliderData));
    w->vtable.render = rs_render;
    w->vtable.handle_event = rs_handle_event;
    w->vtable.is_dirty = rs_is_dirty;
    w->vtable.clear_dirty = rs_clear_dirty;
    w->vtable.destroy = rs_destroy;
    RangeSliderData *d = (RangeSliderData *)(w + 1);
    d->title = strdup(title);
    d->min = min; d->max = max; d->value = value;
    d->label = strdup(label);
    d->dirty = true;
    return w;
}