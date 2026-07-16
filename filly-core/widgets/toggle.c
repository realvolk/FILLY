#include <stdio.h>
#include "toggle.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *title;
    char *label;
    bool value;
    bool focused;
    bool dirty;
} ToggleData;

static void toggle_render(Widget *self, Rect area, RenderTree *out) {
    ToggleData *d = (ToggleData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = 40, box_h = 5;
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    RenderTree *children = calloc(3, sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = strdup(d->title);
    children[0].text.align = ALIGN_CENTER;
    children[0].text.style = textstyle_selected();

    children[1].type = RNODE_TEXT;
    children[1].rect = rect_new(1, 2, box_w - 2, 1);
    char switch_line[128];
    snprintf(switch_line, sizeof(switch_line), "[ %s ] %s", d->value ? "ON" : "OFF", d->label);
    children[1].text.content = strdup(switch_line);
    children[1].text.align = ALIGN_CENTER;
    children[1].text.style = textstyle_normal();

    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = strdup("Space:toggle  Enter:confirm  Esc:cancel");
    children[2].text.align = ALIGN_CENTER;
    children[2].text.style = textstyle_muted();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 3;
}

static EventResult toggle_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    ToggleData *d = (ToggleData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_CHAR:
            if (ev->ch == ' ') { d->value = !d->value; d->dirty = true; }
            return event_result_handled();
        case KEY_ENTER:
            return event_result_response((WidgetResponse){ .result = cJSON_CreateBool(d->value ? 1 : 0), .cancelled = false, .error = NULL });
        default: return event_result_unhandled();
    }
}

static bool toggle_is_dirty(Widget *self) { return ((ToggleData *)(self + 1))->dirty; }
static void toggle_clear_dirty(Widget *self) { ((ToggleData *)(self + 1))->dirty = false; }
static void toggle_destroy(Widget *self) {
    ToggleData *d = (ToggleData *)(self + 1);
    free(d->title); free(d->label);
}

Widget *toggle_widget_new(const char *title, const char *label, bool default_val) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(ToggleData));
    w->vtable.render = toggle_render;
    w->vtable.handle_event = toggle_handle_event;
    w->vtable.is_dirty = toggle_is_dirty;
    w->vtable.clear_dirty = toggle_clear_dirty;
    w->vtable.destroy = toggle_destroy;
    ToggleData *d = (ToggleData *)(w + 1);
    d->title = title ? strdup(title) : NULL;
    d->label = strdup(label);
    d->value = default_val;
    d->focused = true;
    d->dirty = true;
    return w;
}