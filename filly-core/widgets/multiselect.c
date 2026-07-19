#include <stdio.h>
#include "multiselect.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char *title;
    char *message;
    char **choices;
    int count;
    char *query;
    int cursor;
    bool *selected;
    char *placeholder;
    int min_sel;
    int max_sel;
    int *filtered;
    int filtered_count;
    bool dirty;
} MultiselectData;

static void ms_update(MultiselectData *d) {
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
    if (d->cursor >= d->filtered_count) d->cursor = d->filtered_count > 0 ? d->filtered_count - 1 : 0;
}

static void multiselect_render(Widget *self, Rect area, RenderTree *out) {
    MultiselectData *d = (MultiselectData *)(self + 1);
    ms_update(d);
    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.7f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.8f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    out->accessible.role = strdup("dialog");
    out->accessible.label = strdup(d->title ? d->title : "Multiselect");

    RenderTree *children = calloc(5 + 1, sizeof(RenderTree));
    int idx = 0;
    if (d->title && strlen(d->title)) {
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, 0, box_w - 2, 1);
        children[idx].text.content = strdup(d->title);
        children[idx].text.align = ALIGN_CENTER;
        children[idx].text.style = textstyle_selected();
        children[idx].accessible.role = strdup("heading");
        children[idx].accessible.label = strdup(d->title);
        idx++;
    }
    int y = 1;
    if (d->message && strlen(d->message)) {
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, y, box_w - 2, 2);
        children[idx].text.content = strdup(d->message);
        children[idx].text.align = ALIGN_LEFT;
        children[idx].text.style = textstyle_normal();
        children[idx].accessible.role = strdup("description");
        idx++; y += 2;
    }
    children[idx].type = RNODE_INPUT;
    children[idx].rect = rect_new(1, y, box_w - 2, 1);
    children[idx].input.text = strlen(d->query) ? malloc(strlen(d->query) + 3) : strdup("");
    if (strlen(d->query)) sprintf(children[idx].input.text, "> %s", d->query);
    children[idx].input.cursor = strlen(d->query) + 2;
    children[idx].input.placeholder = d->placeholder ? strdup(d->placeholder) : strdup("Type to filter...");
    children[idx].input.masked = false;
    children[idx].accessible.role = strdup("searchbox");
    children[idx].accessible.label = strdup(d->placeholder ? d->placeholder : "Filter");
    idx++; y++;

    int list_h = box_h - y - 2;
    children[idx].type = RNODE_LIST;
    children[idx].rect = rect_new(1, y, box_w - 2, list_h > 0 ? list_h : 1);
    children[idx].list.item_count = d->filtered_count;
    children[idx].list.items = malloc(d->filtered_count * sizeof(ListItem));
    for (int i = 0; i < d->filtered_count; i++) {
        char label[1024];
        int orig = d->filtered[i];
        snprintf(label, sizeof(label), " %s %s", d->selected[orig] ? "[x]" : "[ ]", d->choices[orig]);
        children[idx].list.items[i] = listitem_new(label);
    }
    children[idx].list.selected = d->cursor;
    children[idx].list.highlight = textstyle_selected();
    children[idx].accessible.role = strdup("listbox");
    idx++;

    int sel_count = 0;
    for (int i = 0; i < d->count; i++) if (d->selected[i]) sel_count++;
    children[idx].type = RNODE_TEXT;
    children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    char footer_text[256];
    snprintf(footer_text, sizeof(footer_text), "Selected: %d/%d  Space:toggle  Enter:confirm  Esc:cancel", sel_count, d->count);
    children[idx].text.content = strdup(footer_text);
    children[idx].text.align = ALIGN_CENTER;
    children[idx].text.style = textstyle_muted();
    idx++;

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = idx;
}

static EventResult multiselect_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    MultiselectData *d = (MultiselectData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_UP:
            if (d->cursor > 0) { d->cursor--; d->dirty = true; }
            return event_result_handled();
        case KEY_DOWN:
            if (d->cursor + 1 < d->filtered_count) { d->cursor++; d->dirty = true; }
            return event_result_handled();
        case KEY_CHAR:
            if (ev->ch == ' ') {
                if (d->filtered_count > 0 && d->cursor < d->filtered_count) {
                    int orig = d->filtered[d->cursor];
                    int sel_count = 0;
                    for (int i = 0; i < d->count; i++) if (d->selected[i]) sel_count++;
                    if (d->selected[orig]) { if (sel_count > d->min_sel) d->selected[orig] = false; }
                    else { if (sel_count < d->max_sel) d->selected[orig] = true; }
                    d->dirty = true;
                }
            } else {
                int len = strlen(d->query);
                d->query = realloc(d->query, len + 2);
                d->query[len] = ev->ch; d->query[len + 1] = '\0';
                d->cursor = 0; d->dirty = true;
            }
            return event_result_handled();
        case KEY_BACKSPACE:
            if (strlen(d->query) > 0) { d->query[strlen(d->query) - 1] = '\0'; d->cursor = 0; d->dirty = true; }
            return event_result_handled();
        case KEY_ENTER: {
            int sel_count = 0;
            for (int i = 0; i < d->count; i++) if (d->selected[i]) sel_count++;
            if (sel_count >= d->min_sel && sel_count <= d->max_sel) {
                cJSON *arr = cJSON_CreateArray();
                for (int i = 0; i < d->count; i++)
                    if (d->selected[i]) cJSON_AddItemToArray(arr, cJSON_CreateString(d->choices[i]));
                return event_result_response((WidgetResponse){ .result = arr, .cancelled = false, .error = NULL });
            }
            return event_result_handled();
        }
        default: return event_result_unhandled();
    }
}

static bool multiselect_is_dirty(Widget *self) { return ((MultiselectData *)(self + 1))->dirty; }
static void multiselect_clear_dirty(Widget *self) { ((MultiselectData *)(self + 1))->dirty = false; }
static void multiselect_destroy(Widget *self) {
    MultiselectData *d = (MultiselectData *)(self + 1);
    free(d->title); free(d->message); free(d->query); free(d->placeholder);
    for (int i = 0; i < d->count; i++) free(d->choices[i]);
    free(d->choices); free(d->selected); free(d->filtered);
}

Widget *multiselect_widget_new(const char *title, const char *message, char **choices, int count,
                                const char *placeholder, int min_sel, int max_sel) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(MultiselectData));
    w->vtable.render = multiselect_render;
    w->vtable.handle_event = multiselect_handle_event;
    w->vtable.is_dirty = multiselect_is_dirty;
    w->vtable.clear_dirty = multiselect_clear_dirty;
    w->vtable.destroy = multiselect_destroy;
    MultiselectData *d = (MultiselectData *)(w + 1);
    d->title = title ? strdup(title) : NULL;
    d->message = message ? strdup(message) : NULL;
    d->choices = malloc(count * sizeof(char *));
    for (int i = 0; i < count; i++) d->choices[i] = strdup(choices[i]);
    d->count = count;
    d->query = strdup("");
    d->cursor = 0;
    d->selected = calloc(count, sizeof(bool));
    d->placeholder = placeholder ? strdup(placeholder) : strdup("Type to filter...");
    d->min_sel = min_sel;
    d->max_sel = max_sel > 0 ? max_sel : count;
    d->filtered = NULL;
    d->filtered_count = 0;
    d->dirty = true;
    return w;
}