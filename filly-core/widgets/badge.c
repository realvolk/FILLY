#include <stdio.h>
#include "badge.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *text;
    bool dirty;
} BadgeData;

static void badge_render(Widget *self, Rect area, RenderTree *out) {
    BadgeData *d = (BadgeData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->type = RNODE_BADGE;
    out->rect = area;
    out->badge.text = strdup(d->text);
    TextStyle s = { .fg = strdup("0"), .bg = strdup("2"), .bold = true };
    out->badge.style = s;
    out->accessible.role = strdup("status");
    out->accessible.label = strdup(d->text);
}

static EventResult badge_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)self; (void)backend;
    if (ev->type == EVENT_KEY) return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false, .error = NULL });
    return event_result_unhandled();
}

static bool badge_is_dirty(Widget *self) { return ((BadgeData *)(self + 1))->dirty; }
static void badge_clear_dirty(Widget *self) { ((BadgeData *)(self + 1))->dirty = false; }
static void badge_destroy(Widget *self) { free(((BadgeData *)(self + 1))->text); }

Widget *badge_widget_new(const char *text) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(BadgeData));
    w->vtable.render = badge_render; w->vtable.handle_event = badge_handle_event;
    w->vtable.is_dirty = badge_is_dirty; w->vtable.clear_dirty = badge_clear_dirty; w->vtable.destroy = badge_destroy;
    BadgeData *d = (BadgeData *)(w + 1); d->text = strdup(text); d->dirty = true;
    return w;
}