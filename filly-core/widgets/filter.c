#include <stdio.h>
#include "filter.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char *title;
    char *message;
    char **choices;
    int count;
    char *query;
    int selected;
    char *placeholder;
    int *filtered;
    int filtered_count;
    bool dirty;
} FilterData;

static void filter_update(FilterData *d) {
    free(d->filtered);
    if (strlen(d->query) == 0) {
        d->filtered = malloc(d->count * sizeof(int));
        d->filtered_count = d->count;
        for (int i = 0; i < d->count; i++) d->filtered[i] = i;
    } else {
        d->filtered = malloc(d->count * sizeof(int));
        d->filtered_count = 0;
        char *ql = strdup(d->query);
        for (int i = 0; ql[i]; i++) ql[i] = tolower(ql[i]);
        for (int i = 0; i < d->count; i++) {
            char *cl = strdup(d->choices[i]);
            for (int j = 0; cl[j]; j++) cl[j] = tolower(cl[j]);
            if (strstr(cl, ql)) d->filtered[d->filtered_count++] = i;
            free(cl);
        }
        free(ql);
    }
    if (d->selected >= d->filtered_count) d->selected = d->filtered_count > 0 ? d->filtered_count - 1 : 0;
}

static void filter_render(Widget *self, Rect area, RenderTree *out) {
    FilterData *d = (FilterData *)(self + 1);
    filter_update(d);
    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.7f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.8f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    RenderTree *children = calloc(4 + 1, sizeof(RenderTree));
    int idx = 0;
    if (d->title && strlen(d->title)) {
        RenderTree *t = &children[idx++];
        t->type = RNODE_TEXT;
        t->rect = rect_new(1, 0, box_w - 2, 1);
        t->text.content = strdup(d->title);
        t->text.align = ALIGN_CENTER;
        t->text.style = textstyle_selected();
    }
    RenderTree *search = &children[idx++];
    search->type = RNODE_INPUT;
    search->rect = rect_new(1, 1, box_w - 2, 1);
    search->input.text = strlen(d->query) ? malloc(strlen(d->query) + 3) : strdup("");
    if (strlen(d->query)) sprintf(search->input.text, "> %s", d->query);
    search->input.cursor = strlen(d->query) + 2;
    search->input.placeholder = d->placeholder ? strdup(d->placeholder) : strdup("Type to filter...");
    search->input.masked = false;

    int list_h = box_h - 3;
    RenderTree *list = &children[idx++];
    list->type = RNODE_LIST;
    list->rect = rect_new(1, 2, box_w - 2, list_h > 0 ? list_h : 1);
    list->list.item_count = d->filtered_count;
    list->list.items = malloc(d->filtered_count * sizeof(ListItem));
    for (int i = 0; i < d->filtered_count; i++)
        list->list.items[i] = listitem_new(d->choices[d->filtered[i]]);
    list->list.selected = d->selected;
    list->list.highlight = textstyle_selected();

    RenderTree *footer = &children[idx++];
    footer->type = RNODE_TEXT;
    footer->rect = rect_new(1, box_h - 2, box_w - 2, 1);
    footer->text.content = strdup("Type:filter  Up/Down:move  Enter:select  Esc:cancel");
    footer->text.align = ALIGN_CENTER;
    footer->text.style = textstyle_muted();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = idx;
}

static EventResult filter_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    FilterData *d = (FilterData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_UP:
            if (d->selected > 0) { d->selected--; d->dirty = true; }
            return event_result_handled();
        case KEY_DOWN:
            if (d->selected + 1 < d->filtered_count) { d->selected++; d->dirty = true; }
            return event_result_handled();
        case KEY_ENTER:
            if (d->filtered_count > 0 && d->selected < d->filtered_count)
                return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->choices[d->filtered[d->selected]]), .cancelled = false, .error = NULL });
            return event_result_handled();
        case KEY_CHAR: {
            int len = strlen(d->query);
            d->query = realloc(d->query, len + 2);
            d->query[len] = ev->ch; d->query[len + 1] = '\0';
            d->selected = 0; d->dirty = true;
            return event_result_handled();
        }
        case KEY_BACKSPACE:
            if (strlen(d->query) > 0) { d->query[strlen(d->query) - 1] = '\0'; d->selected = 0; d->dirty = true; }
            return event_result_handled();
        default:
            return event_result_unhandled();
    }
}

static bool filter_is_dirty(Widget *self) { return ((FilterData *)(self + 1))->dirty; }
static void filter_clear_dirty(Widget *self) { ((FilterData *)(self + 1))->dirty = false; }
static void filter_destroy(Widget *self) {
    FilterData *d = (FilterData *)(self + 1);
    free(d->title); free(d->message); free(d->query); free(d->placeholder);
    for (int i = 0; i < d->count; i++) free(d->choices[i]);
    free(d->choices); free(d->filtered);
}

Widget *filter_widget_new(const char *title, const char *message, char **choices, int count, const char *placeholder) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(FilterData));
    w->vtable.render = filter_render;
    w->vtable.handle_event = filter_handle_event;
    w->vtable.is_dirty = filter_is_dirty;
    w->vtable.clear_dirty = filter_clear_dirty;
    w->vtable.destroy = filter_destroy;
    FilterData *d = (FilterData *)(w + 1);
    d->title = title ? strdup(title) : NULL;
    d->message = message ? strdup(message) : NULL;
    d->choices = malloc(count * sizeof(char *));
    for (int i = 0; i < count; i++) d->choices[i] = strdup(choices[i]);
    d->count = count;
    d->query = strdup("");
    d->selected = 0;
    d->placeholder = placeholder ? strdup(placeholder) : strdup("Type to filter...");
    d->filtered = NULL;
    d->filtered_count = 0;
    d->dirty = true;
    return w;
}