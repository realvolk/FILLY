#include "toggle.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>

typedef struct { WidgetBase base; char *title, *label; bool value; } ToggleData;
extern Arena *g_session_arena;

static void toggle_render(Widget *self, Rect area, RenderTree *out) {
    ToggleData *d = (ToggleData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = 40, box_h = 5;
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_h > area.h - 2) box_h = area.h - 2;
    RenderTree *children = arena_alloc(g_session_arena, 3 * sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = arena_strdup(g_session_arena, d->title);
    children[0].style_class = "text";
    children[0].state = "title";
    children[1].type = RNODE_TOGGLE;
    children[1].rect = rect_new(1, 2, box_w - 2, 1);
    children[1].toggle.label = arena_strdup(g_session_arena, d->label);
    children[1].toggle.value = d->value;
    children[1].style_class = "toggle";
    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = "Space:toggle  Enter:confirm  Esc:cancel";
    children[2].style_class = "text";
    children[2].state = "muted";
    out->type = RNODE_CONTAINER;
    out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
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
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_CHAR:
            if (ev->ch == ' ') { d->value = !d->value; d->base.dirty = true; }
            return event_result_handled();
        case KEY_ENTER:
            return event_result_response((WidgetResponse){ .result = cJSON_CreateBool(d->value ? 1 : 0), .cancelled = false });
        default:
            return event_result_unhandled();
    }
}

static void toggle_destroy(Widget *self) {
    ToggleData *d = (ToggleData *)(self + 1);
    free(d->title);
    free(d->label);
}

Widget *toggle_widget_new(const char *title, const char *label, bool default_val) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(ToggleData));
    ToggleData *d = (ToggleData *)(w + 1);
    d->base.dirty = true;
    d->title = title ? strdup(title) : NULL;
    d->label = strdup(label);
    d->value = default_val;
    w->vtable.render = toggle_render;
    w->vtable.handle_event = toggle_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = toggle_destroy;
    return w;
}