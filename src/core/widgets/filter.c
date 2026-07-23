#include "filter.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

typedef struct { WidgetBase base; char *title, *message, **choices, *query, *placeholder; int count, selected, *filtered, filtered_count; } FilterData;
extern Arena *g_session_arena;

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
    out->style_class = "container";
    int box_w = (int)(area.w * 0.7f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.8f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    int child_count = (d->title ? 1 : 0) + 3;
    RenderTree *children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree));
    int idx = 0;
    if (d->title && strlen(d->title)) {
        children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, 0, box_w - 2, 1);
        children[idx].text.content = arena_strdup(g_session_arena, d->title);
        children[idx].style_class = "text"; children[idx].state = "title"; idx++;
    }
    children[idx].type = RNODE_INPUT; children[idx].rect = rect_new(1, 1, box_w - 2, 1);
    char query_buf[512];
    snprintf(query_buf, sizeof(query_buf), "> %s", d->query);
    children[idx].input.text = arena_strdup(g_session_arena, query_buf);
    children[idx].input.cursor = strlen(query_buf);
    children[idx].input.placeholder = arena_strdup(g_session_arena, d->placeholder ? d->placeholder : "Type to filter...");
    children[idx].style_class = "input"; idx++;
    int list_h = box_h - 3;
    children[idx].type = RNODE_LIST; children[idx].rect = rect_new(1, 2, box_w - 2, list_h > 0 ? list_h : 1);
    children[idx].list.item_count = d->filtered_count;
    children[idx].list.selected = d->selected;
    children[idx].list.items = arena_alloc(g_session_arena, d->filtered_count * sizeof(ListItem));
    for (int i = 0; i < d->filtered_count; i++)
        children[idx].list.items[i].label = arena_strdup(g_session_arena, d->choices[d->filtered[i]]);
    children[idx].style_class = "list"; idx++;
    children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[idx].text.content = "Type:filter  Up/Down:move  Enter:select  Esc:cancel";
    children[idx].style_class = "text"; children[idx].state = "muted"; idx++;
    out->type = RNODE_CONTAINER;
    out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = children; out->container.child_count = idx;
}

static EventResult filter_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    FilterData *d = (FilterData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_UP: if (d->selected > 0) { d->selected--; d->base.dirty = true; } return event_result_handled();
        case KEY_DOWN: if (d->selected + 1 < d->filtered_count) { d->selected++; d->base.dirty = true; } return event_result_handled();
        case KEY_ENTER: if (d->filtered_count > 0 && d->selected < d->filtered_count) return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->choices[d->filtered[d->selected]]), .cancelled = false }); return event_result_handled();
        case KEY_CHAR: { int len = strlen(d->query); d->query = realloc(d->query, len + 2); d->query[len] = ev->ch; d->query[len+1] = '\0'; d->selected = 0; d->base.dirty = true; return event_result_handled(); }
        case KEY_BACKSPACE: if (strlen(d->query) > 0) { d->query[strlen(d->query)-1] = '\0'; d->selected = 0; d->base.dirty = true; } return event_result_handled();
        default: return event_result_unhandled();
    }
}

static void filter_destroy(Widget *self) { FilterData *d = (FilterData *)(self + 1); free(d->title); free(d->message); free(d->query); free(d->placeholder); for (int i = 0; i < d->count; i++) free(d->choices[i]); free(d->choices); free(d->filtered); }

Widget *filter_widget_new(const char *title, const char *message, char **choices, int count, const char *placeholder) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(FilterData));
    FilterData *d = (FilterData *)(w + 1);
    d->base.dirty = true;
    d->base.accepts_text_input = true;
    d->title = title ? strdup(title) : NULL;
    d->message = message ? strdup(message) : NULL;
    d->choices = malloc(count * sizeof(char *));
    d->count = count;
    for (int i = 0; i < count; i++) d->choices[i] = strdup(choices[i]);
    d->query = strdup("");
    d->selected = 0;
    d->placeholder = placeholder ? strdup(placeholder) : strdup("Type to filter...");
    d->filtered = NULL; d->filtered_count = 0;
    w->vtable.render = filter_render;
    w->vtable.handle_event = filter_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = filter_destroy;
    return w;
}