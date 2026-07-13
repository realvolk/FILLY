#include <string.h>
#include <stdio.h>
#include "separator.h"
#include <stdlib.h>

typedef struct {
    Orientation orientation;
    bool dirty;
} SeparatorData;

static void separator_render(Widget *self, Rect area, RenderTree *out) {
    SeparatorData *d = (SeparatorData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->type = RNODE_SEPARATOR;
    out->rect = area;
    out->separator.orientation = d->orientation;
}

static EventResult separator_handle_event(Widget *self, Event *ev, Backend *backend) { return event_result_unhandled(); }
static bool separator_is_dirty(Widget *self) { return ((SeparatorData *)(self + 1))->dirty; }
static void separator_clear_dirty(Widget *self) { ((SeparatorData *)(self + 1))->dirty = false; }
static void separator_destroy(Widget *self) { }

Widget *separator_widget_new(Orientation orientation) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(SeparatorData));
    w->vtable.render = separator_render;
    w->vtable.handle_event = separator_handle_event;
    w->vtable.is_dirty = separator_is_dirty;
    w->vtable.clear_dirty = separator_clear_dirty;
    w->vtable.destroy = separator_destroy;
    SeparatorData *d = (SeparatorData *)(w + 1);
    d->orientation = orientation;
    d->dirty = true;
    return w;
}