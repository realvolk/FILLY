#include "tooltip.h"
#include "core/widget_base.h"
#include <stdlib.h>
#include <string.h>

typedef struct { WidgetBase base; char *text; } TooltipData;

static void tooltip_render(Widget *self, Rect area, RenderTree *out) {
    TooltipData *d = (TooltipData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->type = RNODE_TEXT;
    out->rect = area;
    out->text.content = d->text;
    out->style_class = "text";
}

static EventResult tooltip_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)self;
    (void)backend;
    if (ev->type == EVENT_KEY)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false });
    return event_result_unhandled();
}

static void tooltip_destroy(Widget *self) {
    free(((TooltipData *)(self + 1))->text);
}

Widget *tooltip_widget_new(const char *text) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(TooltipData));
    TooltipData *d = (TooltipData *)(w + 1);
    d->base.dirty = true;
    d->text = strdup(text);
    w->vtable.render = tooltip_render;
    w->vtable.handle_event = tooltip_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = tooltip_destroy;
    return w;
}