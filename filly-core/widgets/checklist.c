#include <stdio.h>
#include "checklist.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *title;
    char *message;
    char **choices;
    int count;
    bool *checked;
    int selected;
    int min_sel;
    int max_sel;
    bool dirty;
} ChecklistData;

static void checklist_render(Widget *self, Rect area, RenderTree *out) {
    ChecklistData *d = (ChecklistData *)(self + 1);
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
    int list_y = 1;
    if (d->message && strlen(d->message)) {
        RenderTree *t = &children[idx++];
        t->type = RNODE_TEXT;
        t->rect = rect_new(1, 1, box_w - 2, 2);
        t->text.content = strdup(d->message);
        t->text.align = ALIGN_LEFT;
        t->text.style = textstyle_normal();
        list_y = 3;
    }
    int list_h = box_h - list_y - 2;
    RenderTree *list = &children[idx++];
    list->type = RNODE_LIST;
    list->rect = rect_new(1, list_y, box_w - 2, list_h > 0 ? list_h : 1);
    list->list.item_count = d->count;
    list->list.items = malloc(d->count * sizeof(ListItem));
    for (int i = 0; i < d->count; i++) {
        char label[1024];
        snprintf(label, sizeof(label), " %s %s", d->checked[i] ? "[x]" : "[ ]", d->choices[i]);
        list->list.items[i] = listitem_new(label);
    }
    list->list.selected = d->selected;
    list->list.highlight = textstyle_selected();

    RenderTree *footer = &children[idx++];
    footer->type = RNODE_TEXT;
    footer->rect = rect_new(1, box_h - 2, box_w - 2, 1);
    int checked_count = 0;
    for (int i = 0; i < d->count; i++) if (d->checked[i]) checked_count++;
    char footer_text[256];
    snprintf(footer_text, sizeof(footer_text), "Selected: %d/%d  Space:toggle  Enter:confirm  Esc:cancel", checked_count, d->count);
    footer->text.content = strdup(footer_text);
    footer->text.align = ALIGN_CENTER;
    footer->text.style = textstyle_muted();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = idx;
}

static EventResult checklist_handle_event(Widget *self, Event *ev, Backend *backend) {
    ChecklistData *d = (ChecklistData *)(self + 1);
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
        case KEY_CHAR:
            if (ev->ch == ' ') {
                int checked_count = 0;
                for (int i = 0; i < d->count; i++) if (d->checked[i]) checked_count++;
                if (d->checked[d->selected]) {
                    if (checked_count > d->min_sel) { d->checked[d->selected] = false; d->dirty = true; }
                } else {
                    if (checked_count < d->max_sel) { d->checked[d->selected] = true; d->dirty = true; }
                }
            }
            return event_result_handled();
        case KEY_ENTER: {
            int checked_count = 0;
            for (int i = 0; i < d->count; i++) if (d->checked[i]) checked_count++;
            if (checked_count >= d->min_sel && checked_count <= d->max_sel) {
                cJSON *arr = cJSON_CreateArray();
                for (int i = 0; i < d->count; i++)
                    if (d->checked[i]) cJSON_AddItemToArray(arr, cJSON_CreateString(d->choices[i]));
                return event_result_response((WidgetResponse){ .result = arr, .cancelled = false, .error = NULL });
            }
            return event_result_handled();
        }
        default:
            return event_result_unhandled();
    }
}

static bool checklist_is_dirty(Widget *self) { return ((ChecklistData *)(self + 1))->dirty; }
static void checklist_clear_dirty(Widget *self) { ((ChecklistData *)(self + 1))->dirty = false; }
static void checklist_destroy(Widget *self) {
    ChecklistData *d = (ChecklistData *)(self + 1);
    free(d->title); free(d->message);
    for (int i = 0; i < d->count; i++) free(d->choices[i]);
    free(d->choices); free(d->checked);
}

Widget *checklist_widget_new(const char *title, const char *message, char **choices, int count,
                              int min_sel, int max_sel, char **default_choices, int default_count) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(ChecklistData));
    w->vtable.render = checklist_render;
    w->vtable.handle_event = checklist_handle_event;
    w->vtable.is_dirty = checklist_is_dirty;
    w->vtable.clear_dirty = checklist_clear_dirty;
    w->vtable.destroy = checklist_destroy;
    ChecklistData *d = (ChecklistData *)(w + 1);
    d->title = title ? strdup(title) : NULL;
    d->message = message ? strdup(message) : NULL;
    d->choices = malloc(count * sizeof(char *));
    for (int i = 0; i < count; i++) d->choices[i] = strdup(choices[i]);
    d->count = count;
    d->checked = calloc(count, sizeof(bool));
    if (default_choices)
        for (int i = 0; i < default_count; i++)
            for (int j = 0; j < count; j++)
                if (strcmp(d->choices[j], default_choices[i]) == 0) d->checked[j] = true;
    d->selected = 0;
    d->min_sel = min_sel;
    d->max_sel = max_sel > 0 ? max_sel : count;
    d->dirty = true;
    return w;
}