#include "spinner.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>

typedef struct { WidgetBase base; char *message; int frame; } SpinnerData;
extern Arena *g_session_arena;

static void spinner_render(Widget *self, Rect area, RenderTree *out) {
    SpinnerData *d = (SpinnerData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = 40, box_h = 5;
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_h > area.h - 2) box_h = area.h - 2;
    RenderTree *children = arena_alloc(g_session_arena, 2 * sizeof(RenderTree));
    children[0].type = RNODE_SPINNER;
    children[0].rect = rect_new(1, 1, box_w - 2, 1);
    children[0].spinner.message = arena_strdup(g_session_arena, d->message);
    children[0].spinner.frame = d->frame;
    children[0].style_class = "spinner";
    children[1].type = RNODE_TEXT;
    children[1].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[1].text.content = "Esc:cancel";
    children[1].style_class = "text";
    children[1].state = "muted";
    out->type = RNODE_CONTAINER;
    out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 2;
}

static EventResult spinner_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    SpinnerData *d = (SpinnerData *)(self + 1);
    d->frame = (d->frame + 1) % 10;
    d->base.dirty = true;
    if (ev->type == EVENT_KEY && ev->code == KEY_ESC)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
    return event_result_handled();
}

static void spinner_destroy(Widget *self) {
    free(((SpinnerData *)(self + 1))->message);
}

Widget *spinner_widget_new(const char *message) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(SpinnerData));
    SpinnerData *d = (SpinnerData *)(w + 1);
    d->base.dirty = true;
    d->message = strdup(message);
    d->frame = 0;
    w->vtable.render = spinner_render;
    w->vtable.handle_event = spinner_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = spinner_destroy;
    return w;
}