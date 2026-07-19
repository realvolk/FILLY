#include <stdio.h>
#include "context_menu.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char **items;
    int count;
    int selected;
    bool dirty;
} ContextMenuData;

static void cm_render(Widget *self, Rect area, RenderTree *out) {
    ContextMenuData *d = (ContextMenuData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int w = 25, h = d->count + 2; if (w > area.w - 4) w = area.w - 4;
    int x = (area.w - w) / 2, y = (area.h - h) / 2;

    out->accessible.role = strdup("menu");
    out->accessible.label = strdup("Context menu");

    RenderTree *children = calloc(2 + 1, sizeof(RenderTree));
    children[0].type = RNODE_CONTEXT_MENU; children[0].rect = rect_new(0, 0, w - 2, h - 2);
    children[0].context_menu.item_count = d->count; children[0].context_menu.items = malloc(d->count * sizeof(ListItem));
    for (int i = 0; i < d->count; i++) children[0].context_menu.items[i] = listitem_new(d->items[i]);
    children[0].context_menu.selected = d->selected; children[0].context_menu.highlight = textstyle_selected();
    children[0].accessible.role = strdup("listbox");

    children[1].type = RNODE_TEXT; children[1].rect = rect_new(0, h - 2, w - 2, 1);
    children[1].text.content = strdup("Up/Down:move  Enter:select  Esc:cancel");
    children[1].text.align = ALIGN_CENTER; children[1].text.style = textstyle_muted();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(x, y, w, h);
    out->container.bg = strdup("8"); out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = children; out->container.child_count = 2;
}

static EventResult cm_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    ContextMenuData *d = (ContextMenuData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_UP: if (d->selected > 0) { d->selected--; d->dirty = true; } return event_result_handled();
        case KEY_DOWN: if (d->selected + 1 < d->count) { d->selected++; d->dirty = true; } return event_result_handled();
        case KEY_ENTER: return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->items[d->selected]), .cancelled = false, .error = NULL });
        default: return event_result_unhandled();
    }
}

static bool cm_is_dirty(Widget *self) { return ((ContextMenuData *)(self + 1))->dirty; }
static void cm_clear_dirty(Widget *self) { ((ContextMenuData *)(self + 1))->dirty = false; }
static void cm_destroy(Widget *self) { ContextMenuData *d = (ContextMenuData *)(self + 1); for (int i = 0; i < d->count; i++) free(d->items[i]); free(d->items); }

Widget *context_menu_widget_new(char **items, int count) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(ContextMenuData));
    w->vtable.render = cm_render; w->vtable.handle_event = cm_handle_event;
    w->vtable.is_dirty = cm_is_dirty; w->vtable.clear_dirty = cm_clear_dirty; w->vtable.destroy = cm_destroy;
    ContextMenuData *d = (ContextMenuData *)(w + 1); d->items = malloc(count * sizeof(char *)); d->count = count;
    for (int i = 0; i < count; i++) { d->items[i] = strdup(items[i]); }
    d->selected = 0; d->dirty = true;
    return w;
}