#include "badge.h"
#include "core/widget_base.h"
#include <stdlib.h>
#include <string.h>

typedef struct { WidgetBase base; char *text; } BadgeData;

static void badge_render(Widget *self, Rect area, RenderTree *out) {
    BadgeData *d = (BadgeData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->type = RNODE_BADGE;
    out->rect = area;
    out->badge.text = d->text;
    out->style_class = "badge";
}

static EventResult badge_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)self;
    (void)backend;
    if (ev->type == EVENT_KEY)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false });
    return event_result_unhandled();
}

static void badge_destroy(Widget *self) {
    free(((BadgeData *)(self + 1))->text);
}

Widget *badge_widget_new(const char *text) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(BadgeData));
    BadgeData *d = (BadgeData *)(w + 1);
    d->base.dirty = true;
    d->text = strdup(text);
    w->vtable.render = badge_render;
    w->vtable.handle_event = badge_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = badge_destroy;
    return w;
}