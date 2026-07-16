#include <stdio.h>
#include "spinner.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    char *message;
    int frame;
    bool dirty;
} SpinnerData;

static void spinner_render(Widget *self, Rect area, RenderTree *out) {
    SpinnerData *d = (SpinnerData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = 40, box_h = 5;
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    RenderTree *children = calloc(2, sizeof(RenderTree));
    children[0].type = RNODE_SPINNER;
    children[0].rect = rect_new(1, 1, box_w - 2, 1);
    children[0].spinner.message = strdup(d->message);
    children[0].spinner.frame = d->frame;

    children[1].type = RNODE_TEXT;
    children[1].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[1].text.content = strdup("Esc:cancel");
    children[1].text.align = ALIGN_CENTER;
    children[1].text.style = textstyle_muted();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 2;
}

static EventResult spinner_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    SpinnerData *d = (SpinnerData *)(self + 1);
    d->frame = (d->frame + 1) % 10;
    d->dirty = true;
    if (ev->type == EVENT_KEY && ev->code == KEY_ESC)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
    return event_result_handled();
}

static bool spinner_is_dirty(Widget *self) { return ((SpinnerData *)(self + 1))->dirty; }
static void spinner_clear_dirty(Widget *self) { ((SpinnerData *)(self + 1))->dirty = false; }
static void spinner_destroy(Widget *self) { free(((SpinnerData *)(self + 1))->message); }

Widget *spinner_widget_new(const char *message) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(SpinnerData));
    w->vtable.render = spinner_render;
    w->vtable.handle_event = spinner_handle_event;
    w->vtable.is_dirty = spinner_is_dirty;
    w->vtable.clear_dirty = spinner_clear_dirty;
    w->vtable.destroy = spinner_destroy;
    SpinnerData *d = (SpinnerData *)(w + 1);
    d->message = strdup(message);
    d->frame = 0;
    d->dirty = true;
    return w;
}