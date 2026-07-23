#include "gauge.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>

typedef struct { WidgetBase base; char *title, *label; int percent; } GaugeData;
extern Arena *g_session_arena;

static void gauge_render(Widget *self, Rect area, RenderTree *out) {
    GaugeData *d = (GaugeData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = 50, box_h = 6;
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_h > area.h - 2) box_h = area.h - 2;
    RenderTree *children = arena_alloc(g_session_arena, 3 * sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = arena_strdup(g_session_arena, d->title);
    children[0].style_class = "text";
    children[0].state = "title";
    children[1].type = RNODE_GAUGE;
    children[1].rect = rect_new(1, 2, box_w - 2, 2);
    children[1].gauge.percent = d->percent;
    children[1].gauge.label = arena_strdup(g_session_arena, d->label);
    children[1].style_class = "gauge";
    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = "Any key to dismiss";
    children[2].style_class = "text";
    children[2].state = "muted";
    out->type = RNODE_CONTAINER;
    out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 3;
}

static EventResult gauge_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)self;
    (void)backend;
    if (ev->type == EVENT_KEY)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false });
    return event_result_unhandled();
}

static void gauge_destroy(Widget *self) {
    GaugeData *d = (GaugeData *)(self + 1);
    free(d->title);
    free(d->label);
}

Widget *gauge_widget_new(const char *title, int percent, const char *label) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(GaugeData));
    GaugeData *d = (GaugeData *)(w + 1);
    d->base.dirty = true;
    d->title = strdup(title);
    d->percent = percent;
    d->label = strdup(label);
    w->vtable.render = gauge_render;
    w->vtable.handle_event = gauge_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = gauge_destroy;
    return w;
}