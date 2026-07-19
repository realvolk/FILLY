#include <stdio.h>
#include "tooltip.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *text;
    bool dirty;
} TooltipData;

static void tooltip_render(Widget *self, Rect area, RenderTree *out) {
    TooltipData *d = (TooltipData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->type = RNODE_TEXT;
    out->rect = area;
    out->text.content = strdup(d->text);
    out->text.align = ALIGN_LEFT;
    TextStyle s = { .fg = strdup("3") };
    out->text.style = s;
    out->accessible.role = strdup("tooltip");
    out->accessible.label = strdup(d->text);
}

static EventResult tooltip_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)self; (void)backend;
    if (ev->type == EVENT_KEY)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false, .error = NULL });
    return event_result_unhandled();
}

static bool tooltip_is_dirty(Widget *self) { return ((TooltipData *)(self + 1))->dirty; }
static void tooltip_clear_dirty(Widget *self) { ((TooltipData *)(self + 1))->dirty = false; }
static void tooltip_destroy(Widget *self) { free(((TooltipData *)(self + 1))->text); }

Widget *tooltip_widget_new(const char *text) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(TooltipData));
    w->vtable.render = tooltip_render; w->vtable.handle_event = tooltip_handle_event;
    w->vtable.is_dirty = tooltip_is_dirty; w->vtable.clear_dirty = tooltip_clear_dirty; w->vtable.destroy = tooltip_destroy;
    TooltipData *d = (TooltipData *)(w + 1); d->text = strdup(text); d->dirty = true;
    return w;
}