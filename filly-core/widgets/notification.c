#include <stdio.h>
#include "notification.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    char *message;
    int duration;
    time_t start;
    bool dirty;
} NotificationData;

static void notification_render(Widget *self, Rect area, RenderTree *out) {
    NotificationData *d = (NotificationData *)(self + 1);
    memset(out, 0, sizeof(*out));
    if (time(NULL) - d->start > d->duration) {
        out->type = RNODE_TEXT;
        out->rect = rect_new(0, 0, 0, 0);
        out->text.content = strdup("");
        out->text.align = ALIGN_LEFT;
        out->text.style = textstyle_normal();
        return;
    }
    int w = 40, h = 3;
    if (w > area.w - 4) w = area.w - 4;
    int x = area.x + area.w - w - 2, y = area.y + area.h - h - 2;

    RenderTree *children = calloc(1, sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 1, w - 2, 1);
    children[0].text.content = strdup(d->message);
    children[0].text.align = ALIGN_CENTER;
    children[0].text.style = textstyle_accent();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(x, y, w, h);
    out->container.border = BORDER_SINGLE;
    out->container.bg = strdup("8");
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 1;
}

static EventResult notification_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)ev;
    (void)backend;
    NotificationData *d = (NotificationData *)(self + 1);
    if (time(NULL) - d->start > d->duration)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false, .error = NULL });
    return event_result_handled();
}

static bool notification_is_dirty(Widget *self) { return ((NotificationData *)(self + 1))->dirty; }
static void notification_clear_dirty(Widget *self) { ((NotificationData *)(self + 1))->dirty = false; }
static void notification_destroy(Widget *self) { free(((NotificationData *)(self + 1))->message); }

Widget *notification_widget_new(const char *message, int duration_sec) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(NotificationData));
    w->vtable.render = notification_render;
    w->vtable.handle_event = notification_handle_event;
    w->vtable.is_dirty = notification_is_dirty;
    w->vtable.clear_dirty = notification_clear_dirty;
    w->vtable.destroy = notification_destroy;
    NotificationData *d = (NotificationData *)(w + 1);
    d->message = strdup(message);
    d->duration = duration_sec;
    d->start = time(NULL);
    d->dirty = true;
    return w;
}