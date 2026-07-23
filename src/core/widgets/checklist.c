#include "checklist.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { WidgetBase base; char *title, *message, **choices; int count, selected, min_sel, max_sel; bool *checked; } ChecklistData;
extern Arena *g_session_arena;

static void checklist_render(Widget *self, Rect area, RenderTree *out) {
    ChecklistData *d = (ChecklistData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = (int)(area.w * 0.7f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.8f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    int child_count = (d->title ? 1 : 0) + (d->message ? 1 : 0) + 2;
    RenderTree *children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree));
    int idx = 0;
    int list_y = 1;
    if (d->title && strlen(d->title)) {
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, 0, box_w - 2, 1);
        children[idx].text.content = arena_strdup(g_session_arena, d->title);
        children[idx].style_class = "text";
        children[idx].state = "title";
        idx++;
    }
    if (d->message && strlen(d->message)) {
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, 1, box_w - 2, 2);
        children[idx].text.content = arena_strdup(g_session_arena, d->message);
        children[idx].style_class = "text";
        idx++;
        list_y = 3;
    }
    int list_h = box_h - list_y - 2;
    children[idx].type = RNODE_LIST;
    children[idx].rect = rect_new(1, list_y, box_w - 2, list_h > 0 ? list_h : 1);
    children[idx].list.item_count = d->count;
    children[idx].list.selected = d->selected;
    children[idx].list.items = arena_alloc(g_session_arena, d->count * sizeof(ListItem));
    for (int i = 0; i < d->count; i++) {
        char label[1024];
        snprintf(label, sizeof(label), " %s %s", d->checked[i] ? "[x]" : "[ ]", d->choices[i]);
        children[idx].list.items[i].label = arena_strdup(g_session_arena, label);
    }
    children[idx].style_class = "list";
    idx++;
    int checked_count = 0;
    for (int i = 0; i < d->count; i++) if (d->checked[i]) checked_count++;
    children[idx].type = RNODE_TEXT;
    children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    char footer[256];
    snprintf(footer, sizeof(footer), "Selected: %d/%d  Space:toggle  Enter:confirm  Esc:cancel", checked_count, d->count);
    children[idx].text.content = arena_strdup(g_session_arena, footer);
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

static EventResult checklist_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    ChecklistData *d = (ChecklistData *)(self + 1);
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
        case KEY_CHAR:
            if (ev->ch == ' ') {
                int cc = 0;
                for (int i = 0; i < d->count; i++) if (d->checked[i]) cc++;
                if (d->checked[d->selected]) {
                    if (cc > d->min_sel) d->checked[d->selected] = false;
                } else {
                    if (cc < d->max_sel) d->checked[d->selected] = true;
                }
                d->base.dirty = true;
            }
            return event_result_handled();
        case KEY_ENTER: {
            int cc = 0;
            for (int i = 0; i < d->count; i++) if (d->checked[i]) cc++;
            if (cc >= d->min_sel && cc <= d->max_sel) {
                cJSON *arr = cJSON_CreateArray();
                for (int i = 0; i < d->count; i++)
                    if (d->checked[i]) cJSON_AddItemToArray(arr, cJSON_CreateString(d->choices[i]));
                return event_result_response((WidgetResponse){ .result = arr, .cancelled = false });
            }
            return event_result_handled();
        }
        default:
            return event_result_unhandled();
    }
}

static void checklist_destroy(Widget *self) {
    ChecklistData *d = (ChecklistData *)(self + 1);
    free(d->title);
    free(d->message);
    for (int i = 0; i < d->count; i++) free(d->choices[i]);
    free(d->choices);
    free(d->checked);
}

Widget *checklist_widget_new(const char *title, const char *message, char **choices, int count,
                              int min_sel, int max_sel, char **default_choices, int default_count) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(ChecklistData));
    ChecklistData *d = (ChecklistData *)(w + 1);
    d->base.dirty = true;
    d->title = title ? strdup(title) : NULL;
    d->message = message ? strdup(message) : NULL;
    d->choices = malloc(count * sizeof(char *));
    d->count = count;
    for (int i = 0; i < count; i++) d->choices[i] = strdup(choices[i]);
    d->selected = 0;
    d->min_sel = min_sel;
    d->max_sel = max_sel > 0 ? max_sel : count;
    d->checked = calloc(count, sizeof(bool));
    for (int i = 0; i < default_count; i++)
        for (int j = 0; j < count; j++)
            if (strcmp(choices[j], default_choices[i]) == 0) d->checked[j] = true;
    w->vtable.render = checklist_render;
    w->vtable.handle_event = checklist_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = checklist_destroy;
    return w;
}