#include "menu.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>

typedef struct { WidgetBase base; char *title, *message, **choices; int count, selected; } MenuData;
extern Arena *g_session_arena;

static void menu_render(Widget *self, Rect area, RenderTree *out) {
    MenuData *d = (MenuData *)(self + 1);
    memset(out, 0, sizeof(*out)); out->style_class = "container";

    int inner_h = d->count;
    if (d->title && strlen(d->title)) inner_h += 1;
    if (d->message && strlen(d->message)) inner_h += 2;
    inner_h += 1;
    int box_w = (int)(area.w * 0.6f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_w < 30) box_w = 30;
    int box_h = inner_h + 2;
    if (box_h > area.h) box_h = area.h;

    int child_count = (d->title && strlen(d->title) ? 1 : 0) + (d->message && strlen(d->message) ? 1 : 0) + 2;
    RenderTree *children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree));
    int idx = 0;
    int row = 1;
    int content_w = box_w - 2;

    if (d->title && strlen(d->title)) {
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, row, content_w, 1);
        children[idx].text.content = arena_strdup(g_session_arena, d->title);
        children[idx].text.align = ALIGN_CENTER;
        children[idx].style_class = "text";
        children[idx].state = "title";
        idx++;
        row += 1;
    }
    if (d->message && strlen(d->message)) {
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, row, content_w, 2);
        children[idx].text.content = arena_strdup(g_session_arena, d->message);
        children[idx].text.align = ALIGN_CENTER;
        children[idx].style_class = "text";
        idx++;
        row += 2;
    }
    int list_h = d->count;
    children[idx].type = RNODE_LIST;
    children[idx].rect = rect_new(1, row, content_w, list_h);
    children[idx].list.item_count = d->count;
    children[idx].list.selected = d->selected;
    children[idx].list.items = arena_alloc(g_session_arena, d->count * sizeof(ListItem));
    for (int i = 0; i < d->count; i++)
        children[idx].list.items[i].label = arena_strdup(g_session_arena, d->choices[i]);
    children[idx].style_class = "list";
    idx++;
    row += list_h;
    children[idx].type = RNODE_TEXT;
    children[idx].rect = rect_new(1, row, content_w, 1);
    children[idx].text.content = "Up/Down:move  Enter:select  Esc:cancel";
    children[idx].text.align = ALIGN_CENTER;
    children[idx].style_class = "text";
    children[idx].state = "muted";
    idx++;

    out->type = RNODE_CONTAINER;
    out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = idx;
}

static EventResult menu_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend; MenuData *d = (MenuData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_UP: if (d->selected > 0) { d->selected--; d->base.dirty = true; } return event_result_handled();
        case KEY_DOWN: if (d->selected + 1 < d->count) { d->selected++; d->base.dirty = true; } return event_result_handled();
        case KEY_ENTER: return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->choices[d->selected]), .cancelled = false });
        default: return event_result_unhandled();
    }
}

static void menu_destroy(Widget *self) {
    MenuData *d = (MenuData *)(self + 1);
    free(d->title); free(d->message);
    for (int i = 0; i < d->count; i++) free(d->choices[i]);
    free(d->choices);
}

Widget *menu_widget_new(const char *title, const char *message, char **choices, int count, int def) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(MenuData));
    MenuData *d = (MenuData *)(w + 1);
    d->base.dirty = true;
    d->title = title ? strdup(title) : NULL;
    d->message = message ? strdup(message) : NULL;
    d->choices = malloc(count * sizeof(char *));
    d->count = count;
    for (int i = 0; i < count; i++) d->choices[i] = strdup(choices[i]);
    d->selected = def >= 0 && def < count ? def : 0;
    w->vtable.render = menu_render;
    w->vtable.handle_event = menu_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = menu_destroy;
    return w;
}