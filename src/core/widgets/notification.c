#include "notification.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct { WidgetBase base; char *message; int duration; time_t start; } NotificationData;
extern Arena *g_session_arena;

static void notification_render(Widget *self, Rect area, RenderTree *out) {
    NotificationData *d = (NotificationData *)(self + 1);
    memset(out, 0, sizeof(*out));
    if (time(NULL) - d->start > d->duration) {
        out->type = RNODE_TEXT;
        out->rect = rect_new(0, 0, 0, 0);
        out->text.content = "";
        return;
    }
    int w = 40, h = 3;
    if (w > area.w - 4) w = area.w - 4;
    RenderTree *children = arena_alloc(g_session_arena, 1 * sizeof(RenderTree));
    children[0].type = RNODE_TOAST;
    children[0].rect = rect_new(1, 1, w - 2, 1);
    children[0].toast.message = arena_strdup(g_session_arena, d->message);
    children[0].style_class = "toast";
    out->type = RNODE_CONTAINER;
    out->rect = rect_new(area.x + area.w - w - 2, area.y + area.h - h - 2, w, h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 1;
}

static EventResult notification_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)ev;
    (void)backend;
    NotificationData *d = (NotificationData *)(self + 1);
    if (time(NULL) - d->start > d->duration)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false });
    return event_result_handled();
}

static void notification_destroy(Widget *self) {
    free(((NotificationData *)(self + 1))->message);
}

Widget *notification_widget_new(const char *message, int duration_sec) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(NotificationData));
    NotificationData *d = (NotificationData *)(w + 1);
    d->base.dirty = true;
    d->message = strdup(message);
    d->duration = duration_sec;
    d->start = time(NULL);
    w->vtable.render = notification_render;
    w->vtable.handle_event = notification_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = notification_destroy;
    return w;
}