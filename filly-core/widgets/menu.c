#include <stdio.h>
#include "menu.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *title;
    char *message;
    char **choices;
    int count;
    int selected;
    bool dirty;
} MenuData;

static void menu_render(Widget *self, Rect area, RenderTree *out) {
    MenuData *d = (MenuData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.6f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.8f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2;
    int box_y = (area.h - box_h) / 2;

    RenderTree *children = calloc(3 + 1, sizeof(RenderTree));
    int child_idx = 0;

    if (d->title && strlen(d->title)) {
        RenderTree *t = &children[child_idx++];
        t->type = RNODE_TEXT;
        t->rect = rect_new(1, 0, box_w - 2, 1);
        t->text.content = strdup(d->title);
        t->text.align = ALIGN_CENTER;
        t->text.style = textstyle_selected();
        t->text.style.bold = true;
    }
    int list_y = 1;
    if (d->message && strlen(d->message)) {
        RenderTree *t = &children[child_idx++];
        t->type = RNODE_TEXT;
        t->rect = rect_new(1, 1, box_w - 2, 2);
        t->text.content = strdup(d->message);
        t->text.align = ALIGN_LEFT;
        t->text.style = textstyle_normal();
        list_y = 3;
    }
    int list_h = box_h - list_y - 2;
    RenderTree *list = &children[child_idx++];
    list->type = RNODE_LIST;
    list->rect = rect_new(1, list_y, box_w - 2, list_h > 0 ? list_h : 1);
    list->list.item_count = d->count;
    list->list.items = malloc(d->count * sizeof(ListItem));
    for (int i = 0; i < d->count; i++)
        list->list.items[i] = listitem_new(d->choices[i]);
    list->list.selected = d->selected;
    list->list.highlight = textstyle_selected();

    RenderTree *footer = &children[child_idx++];
    footer->type = RNODE_TEXT;
    footer->rect = rect_new(1, box_h - 2, box_w - 2, 1);
    footer->text.content = strdup("Up/Down:move  Enter:select  Esc:cancel");
    footer->text.align = ALIGN_CENTER;
    footer->text.style = textstyle_muted();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = child_idx;
}

static EventResult menu_handle_event(Widget *self, Event *ev, Backend *backend) {
    MenuData *d = (MenuData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_UP:
            if (d->selected > 0) { d->selected--; d->dirty = true; }
            return event_result_handled();
        case KEY_DOWN:
            if (d->selected + 1 < d->count) { d->selected++; d->dirty = true; }
            return event_result_handled();
        case KEY_ENTER: {
            cJSON *res = cJSON_CreateString(d->choices[d->selected]);
            return event_result_response((WidgetResponse){ .result = res, .cancelled = false, .error = NULL });
        }
        default:
            return event_result_unhandled();
    }
}

static bool menu_is_dirty(Widget *self) { return ((MenuData *)(self + 1))->dirty; }
static void menu_clear_dirty(Widget *self) { ((MenuData *)(self + 1))->dirty = false; }
static void menu_destroy(Widget *self) {
    MenuData *d = (MenuData *)(self + 1);
    free(d->title);
    free(d->message);
    for (int i = 0; i < d->count; i++) free(d->choices[i]);
    free(d->choices);
}

Widget *menu_widget_new(const char *title, const char *message, char **choices, int count, int def) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(MenuData));
    w->vtable.render = menu_render;
    w->vtable.handle_event = menu_handle_event;
    w->vtable.is_dirty = menu_is_dirty;
    w->vtable.clear_dirty = menu_clear_dirty;
    w->vtable.destroy = menu_destroy;
    MenuData *d = (MenuData *)(w + 1);
    d->title = title ? strdup(title) : NULL;
    d->message = message ? strdup(message) : NULL;
    d->choices = malloc(count * sizeof(char *));
    for (int i = 0; i < count; i++) d->choices[i] = strdup(choices[i]);
    d->count = count;
    d->selected = def >= 0 && def < count ? def : 0;
    d->dirty = true;
    return w;
}