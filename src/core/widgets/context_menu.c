#include "context_menu.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>

typedef struct { WidgetBase base; char **items; int count, selected; } ContextMenuData;
extern Arena *g_session_arena;

static void cm_render(Widget *self, Rect area, RenderTree *out) {
    ContextMenuData *d = (ContextMenuData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = 25, box_h = d->count + 2;
    if (box_w > area.w - 4) box_w = area.w - 4;
    if (box_h > area.h - 2) box_h = area.h - 2;
    RenderTree *children = arena_alloc(g_session_arena, 2 * sizeof(RenderTree));
    children[0].type = RNODE_CONTEXT_MENU;
    children[0].rect = rect_new(0, 0, box_w - 2, box_h - 2);
    children[0].context_menu.item_count = d->count;
    children[0].context_menu.selected = d->selected;
    children[0].context_menu.items = arena_alloc(g_session_arena, d->count * sizeof(ListItem));
    for (int i = 0; i < d->count; i++)
        children[0].context_menu.items[i].label = arena_strdup(g_session_arena, d->items[i]);
    children[0].style_class = "context_menu";
    children[1].type = RNODE_TEXT;
    children[1].rect = rect_new(0, box_h - 2, box_w - 2, 1);
    children[1].text.content = "Up/Down:move  Enter:select  Esc:cancel";
    children[1].style_class = "text";
    children[1].state = "muted";
    out->type = RNODE_CONTAINER;
    out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 2;
}

static EventResult cm_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    ContextMenuData *d = (ContextMenuData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_UP:
            if (d->selected > 0) { d->selected--; d->base.dirty = true; }
            return event_result_handled();
        case KEY_DOWN:
            if (d->selected + 1 < d->count) { d->selected++; d->base.dirty = true; }
            return event_result_handled();
        case KEY_ENTER:
            return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->items[d->selected]), .cancelled = false });
        default:
            return event_result_unhandled();
    }
}

static void cm_destroy(Widget *self) {
    ContextMenuData *d = (ContextMenuData *)(self + 1);
    for (int i = 0; i < d->count; i++) free(d->items[i]);
    free(d->items);
}

Widget *context_menu_widget_new(char **items, int count) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(ContextMenuData));
    ContextMenuData *d = (ContextMenuData *)(w + 1);
    d->base.dirty = true;
    d->items = malloc(count * sizeof(char *));
    d->count = count;
    for (int i = 0; i < count; i++) d->items[i] = strdup(items[i]);
    d->selected = 0;
    w->vtable.render = cm_render;
    w->vtable.handle_event = cm_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = cm_destroy;
    return w;
}