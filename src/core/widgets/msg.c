#include "msg.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>

typedef struct { WidgetBase base; char *title, *message; } MsgData;
extern Arena *g_session_arena;

static void msg_render(Widget *self, Rect area, RenderTree *out) {
    MsgData *d = (MsgData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";

    int msg_lines = 1;
    for (const char *p = d->message; *p; p++) if (*p == '\n') msg_lines++;
    int inner_h = 1 + msg_lines + 1;
    int box_w = (int)(area.w * 0.6f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_w < 30) box_w = 30;
    int box_h = inner_h + 2;
    if (box_h > area.h) box_h = area.h;

    RenderTree *children = arena_alloc(g_session_arena, 3 * sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 1, box_w - 2, 1);
    children[0].text.content = arena_strdup(g_session_arena, d->title);
    children[0].text.align = ALIGN_CENTER;
    children[0].style_class = "text";
    children[0].state = "title";

    children[1].type = RNODE_TEXT;
    children[1].rect = rect_new(1, 2, box_w - 2, msg_lines);
    children[1].text.content = arena_strdup(g_session_arena, d->message);
    children[1].text.align = ALIGN_CENTER;
    children[1].style_class = "text";

    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, 2 + msg_lines, box_w - 2, 1);
    children[2].text.content = "Any key to continue";
    children[2].text.align = ALIGN_CENTER;
    children[2].style_class = "text";
    children[2].state = "muted";

    out->type = RNODE_CONTAINER;
    out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 3;
}

static EventResult msg_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)self; (void)backend;
    if (ev->type == EVENT_KEY)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false });
    return event_result_unhandled();
}

static void msg_destroy(Widget *self) {
    MsgData *d = (MsgData *)(self + 1);
    free(d->title);
    free(d->message);
}

Widget *msg_widget_new(const char *title, const char *message) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(MsgData));
    MsgData *d = (MsgData *)(w + 1);
    d->base.dirty = true;
    d->title = strdup(title);
    d->message = strdup(message);
    w->vtable.render = msg_render;
    w->vtable.handle_event = msg_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = msg_destroy;
    return w;
}