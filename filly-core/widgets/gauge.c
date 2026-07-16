#include "gauge.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char *title;
    int percent;
    char *label;
    bool dirty;
} GaugeData;

static void gauge_render(Widget *self, Rect area, RenderTree *out) {
    GaugeData *d = (GaugeData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = 50, box_h = 6;
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    RenderTree *children = calloc(3 + 1, sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = strdup(d->title);
    children[0].text.align = ALIGN_CENTER;
    children[0].text.style = textstyle_selected();

    children[1].type = RNODE_GAUGE;
    children[1].rect = rect_new(1, 2, box_w - 2, 2);
    children[1].gauge.percent = d->percent;
    children[1].gauge.label = strdup(d->label);
    children[1].gauge.style = textstyle_accent();

    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = strdup("Any key to dismiss");
    children[2].text.align = ALIGN_CENTER;
    children[2].text.style = textstyle_muted();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 3;
}

static EventResult gauge_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)self;
    (void)backend;
    if (ev->type == EVENT_KEY)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false, .error = NULL });
    return event_result_unhandled();
}

static bool gauge_is_dirty(Widget *self) { return ((GaugeData *)(self + 1))->dirty; }
static void gauge_clear_dirty(Widget *self) { ((GaugeData *)(self + 1))->dirty = false; }
static void gauge_destroy(Widget *self) {
    GaugeData *d = (GaugeData *)(self + 1);
    free(d->title); free(d->label);
}

Widget *gauge_widget_new(const char *title, int percent, const char *label) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(GaugeData));
    w->vtable.render = gauge_render;
    w->vtable.handle_event = gauge_handle_event;
    w->vtable.is_dirty = gauge_is_dirty;
    w->vtable.clear_dirty = gauge_clear_dirty;
    w->vtable.destroy = gauge_destroy;
    GaugeData *d = (GaugeData *)(w + 1);
    d->title = strdup(title);
    d->percent = percent;
    d->label = strdup(label);
    d->dirty = true;
    return w;
}