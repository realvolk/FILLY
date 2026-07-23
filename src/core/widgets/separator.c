#include "separator.h"
#include "core/widget_base.h"
#include <stdlib.h>
#include <string.h>

typedef struct { WidgetBase base; Orientation orientation; } SeparatorData;

static void separator_render(Widget *self, Rect area, RenderTree *out) {
    SeparatorData *d = (SeparatorData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->type = RNODE_SEPARATOR;
    out->rect = area;
    out->separator.orientation = d->orientation;
    out->style_class = "separator";
}

static EventResult separator_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)self;
    (void)backend;
    if (ev->type == EVENT_KEY && ev->code == KEY_ESC)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false });
    return event_result_unhandled();
}

static void separator_destroy(Widget *self) { (void)self; }

Widget *separator_widget_new(Orientation orientation) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(SeparatorData));
    SeparatorData *d = (SeparatorData *)(w + 1);
    d->base.dirty = true;
    d->orientation = orientation;
    w->vtable.render = separator_render;
    w->vtable.handle_event = separator_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = separator_destroy;
    return w;
}