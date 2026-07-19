#include <stdio.h>
#include "msg.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *title;
    char *message;
    bool dirty;
} MsgData;

static void msg_render(Widget *self, Rect area, RenderTree *out) {
    MsgData *d = (MsgData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.6f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.4f);
    if (box_h < 5) box_h = 5;
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    out->accessible.role = strdup("dialog");
    out->accessible.label = strdup(d->title ? d->title : "Message");

    RenderTree *children = calloc(3 + 1, sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = strdup(d->title);
    children[0].text.align = ALIGN_CENTER;
    children[0].text.style = textstyle_selected();
    children[0].accessible.role = strdup("heading");
    children[0].accessible.label = strdup(d->title);

    children[1].type = RNODE_TEXT;
    int msg_h = 1; for (const char *p = d->message; *p; p++) if (*p == '\n') msg_h++;
    children[1].rect = rect_new(1, (box_h - msg_h)/2, box_w - 2, msg_h);
    children[1].text.content = strdup(d->message);
    children[1].text.align = ALIGN_CENTER;
    children[1].text.style = textstyle_normal();
    children[1].accessible.role = strdup("description");

    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = strdup("Any key to continue");
    children[2].text.align = ALIGN_CENTER;
    children[2].text.style = textstyle_muted();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 3;
}

static EventResult msg_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)self;
    (void)backend;
    if (ev->type == EVENT_KEY)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false, .error = NULL });
    return event_result_unhandled();
}

static bool msg_is_dirty(Widget *self) { return ((MsgData *)(self + 1))->dirty; }
static void msg_clear_dirty(Widget *self) { ((MsgData *)(self + 1))->dirty = false; }
static void msg_destroy(Widget *self) {
    MsgData *d = (MsgData *)(self + 1);
    free(d->title); free(d->message);
}

Widget *msg_widget_new(const char *title, const char *message) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(MsgData));
    w->vtable.render = msg_render;
    w->vtable.handle_event = msg_handle_event;
    w->vtable.is_dirty = msg_is_dirty;
    w->vtable.clear_dirty = msg_clear_dirty;
    w->vtable.destroy = msg_destroy;
    MsgData *d = (MsgData *)(w + 1);
    d->title = strdup(title);
    d->message = strdup(message);
    d->dirty = true;
    return w;
}