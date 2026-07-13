#include <stdio.h>
#include "split_panes.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    Orientation orientation;
    Widget *first;
    Widget *second;
    int split_position;
    int active_pane;
    bool dirty;
} SplitPanesData;

static void sp_render(Widget *self, Rect area, RenderTree *out) {
    SplitPanesData *d = (SplitPanesData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->type = RNODE_SPLIT_PANES;
    out->rect = area;
    out->split_panes.orientation = d->orientation;
    out->split_panes.split_position = d->split_position > 0 ? d->split_position : (d->orientation == ORIENT_HORIZONTAL ? area.w / 2 : area.h / 2);
    out->split_panes.first = malloc(sizeof(RenderTree));
    out->split_panes.second = malloc(sizeof(RenderTree));
    d->first->vtable.render(d->first, area, out->split_panes.first);
    d->second->vtable.render(d->second, area, out->split_panes.second);
}

static EventResult sp_handle_event(Widget *self, Event *ev, Backend *backend) {
    SplitPanesData *d = (SplitPanesData *)(self + 1);
    if (ev->type == EVENT_KEY) {
        if (ev->code == KEY_ESC)
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false, .error = NULL });
        if (ev->code == KEY_F1) { d->active_pane = 0; d->dirty = true; return event_result_handled(); }
        if (ev->code == KEY_F2) { d->active_pane = 1; d->dirty = true; return event_result_handled(); }
    }
    EventResult r = d->active_pane == 0 ? d->first->vtable.handle_event(d->first, ev, backend) : d->second->vtable.handle_event(d->second, ev, backend);
    if (r.type != EVENT_RESULT_UNHANDLED) d->dirty = true;
    return r;
}

static bool sp_is_dirty(Widget *self) { return ((SplitPanesData *)(self + 1))->dirty; }
static void sp_clear_dirty(Widget *self) { ((SplitPanesData *)(self + 1))->dirty = false; }
static void sp_destroy(Widget *self) {
    SplitPanesData *d = (SplitPanesData *)(self + 1);
    widget_destroy(d->first); widget_destroy(d->second);
}

Widget *split_panes_widget_new(Orientation orientation, Widget *first, Widget *second) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(SplitPanesData));
    w->vtable.render = sp_render;
    w->vtable.handle_event = sp_handle_event;
    w->vtable.is_dirty = sp_is_dirty;
    w->vtable.clear_dirty = sp_clear_dirty;
    w->vtable.destroy = sp_destroy;
    SplitPanesData *d = (SplitPanesData *)(w + 1);
    d->orientation = orientation;
    d->first = first; d->second = second;
    d->split_position = 0;
    d->active_pane = 0;
    d->dirty = true;
    return w;
}