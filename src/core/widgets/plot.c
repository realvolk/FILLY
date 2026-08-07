#include "plot.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

typedef struct {
    WidgetBase base;
    char *type;
    double *data;
    int data_count;
    char **labels;
    int label_count;
} PlotData;

extern Arena *g_session_arena;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void plot_render(Widget *self, RenderTree *out) {
    PlotData *d = (PlotData *)(self + 1);
    WidgetBase *base = (WidgetBase *)(self + 1);
    Rect area = base->render_area;
    memset(out, 0, sizeof(*out));
    out->accessible.role = "plot";
    out->accessible.label = d->type;
    out->tab_index = base->tab_index >= 0 ? base->tab_index : -1;

    int ch = (area.w > 200) ? 30 : 1;
    int box_w = (int)(area.w * 0.8f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.8f);
    if (box_h > area.h - 2) box_h = area.h - 2;

    out->type = RNODE_PLOT;
    out->u.plot.type = arena_strdup(g_session_arena, d->type);
    out->u.plot.data = arena_alloc(g_session_arena, d->data_count * sizeof(double));
    memcpy(out->u.plot.data, d->data, d->data_count * sizeof(double));
    out->u.plot.data_count = d->data_count;
    out->u.plot.labels = arena_alloc(g_session_arena, d->label_count * sizeof(char *));
    for (int i = 0; i < d->label_count; i++)
        out->u.plot.labels[i] = arena_strdup(g_session_arena, d->labels[i]);
    out->u.plot.label_count = d->label_count;
    out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->style_class = "plot";

    RenderTree *children = arena_alloc(g_session_arena, 2 * sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(ch, 0, box_w - 2 * ch, ch);
    char title[128];
    snprintf(title, sizeof(title), "%s chart (%d points)", d->type, d->data_count);
    children[0].u.text.content = arena_strdup(g_session_arena, title);
    children[0].style_class = "text";
    children[0].state = "title";

    children[1].type = RNODE_TEXT;
    children[1].rect = rect_new(ch, box_h - 2 * ch, box_w - 2 * ch, ch);
    children[1].u.text.content = "Any key to continue";
    children[1].style_class = "text";
    children[1].state = "muted";

    out->type = RNODE_CONTAINER;
    out->u.container.border = BORDER_SINGLE;
    out->u.container.padding = edgeinsets_zero();
    out->u.container.children = children;
    out->u.container.child_count = 2;
}

static EventResult plot_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)self; (void)backend;
    if (ev->type == EVENT_KEY)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false });
    return event_result_unhandled();
}

static void plot_destroy(Widget *self) {
    PlotData *d = (PlotData *)(self + 1);
    free(d->type);
    free(d->data);
    for (int i = 0; i < d->label_count; i++) free(d->labels[i]);
    free(d->labels);
}

Widget *plot_widget_new(const char *type, double *data, int data_count, char **labels, int label_count) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(PlotData));
    PlotData *d = (PlotData *)(w + 1);
    d->base.dirty = true;
    d->base.tab_index = -1;
    d->type = type ? strdup(type) : strdup("line");
    d->data = malloc(data_count * sizeof(double));
    memcpy(d->data, data, data_count * sizeof(double));
    d->data_count = data_count;
    d->labels = malloc(label_count * sizeof(char *));
    for (int i = 0; i < label_count; i++)
        d->labels[i] = labels ? strdup(labels[i]) : strdup("");
    d->label_count = label_count;
    w->vtable.render = plot_render;
    w->vtable.handle_event = plot_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = plot_destroy;
    return w;
}