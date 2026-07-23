#include "multiselect.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

typedef struct { WidgetBase base; char *title, *message, **choices, *query, *placeholder; int count, cursor, min_sel, max_sel, *filtered, filtered_count; bool *selected; } MultiselectData;
extern Arena *g_session_arena;

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
    out->style_class = "container";
    int box_w = (int)(area.w * 0.7f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.8f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    int child_count = (d->title ? 1 : 0) + (d->message ? 1 : 0) + 3;
    RenderTree *children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree));
    int idx = 0; int y = 0;
    if (d->title && strlen(d->title)) { children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, y, box_w - 2, 1); children[idx].text.content = arena_strdup(g_session_arena, d->title); children[idx].style_class = "text"; children[idx].state = "title"; idx++; y++; }
    if (d->message && strlen(d->message)) { children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, y, box_w - 2, 2); children[idx].text.content = arena_strdup(g_session_arena, d->message); children[idx].style_class = "text"; idx++; y += 2; }
    children[idx].type = RNODE_INPUT; children[idx].rect = rect_new(1, y, box_w - 2, 1);
    char qb[512]; snprintf(qb, sizeof(qb), "> %s", d->query); children[idx].input.text = arena_strdup(g_session_arena, qb); children[idx].input.cursor = strlen(qb);
    children[idx].input.placeholder = arena_strdup(g_session_arena, d->placeholder ? d->placeholder : "Type to filter..."); children[idx].style_class = "input"; idx++; y++;
    int list_h = box_h - y - 2;
    children[idx].type = RNODE_LIST; children[idx].rect = rect_new(1, y, box_w - 2, list_h > 0 ? list_h : 1);
    children[idx].list.item_count = d->filtered_count; children[idx].list.selected = d->cursor;
    children[idx].list.items = arena_alloc(g_session_arena, d->filtered_count * sizeof(ListItem));
    for (int i = 0; i < d->filtered_count; i++) { char label[1024]; int orig = d->filtered[i]; snprintf(label, sizeof(label), " %s %s", d->selected[orig] ? "[x]" : "[ ]", d->choices[orig]); children[idx].list.items[i].label = arena_strdup(g_session_arena, label); }
    children[idx].style_class = "list"; idx++;
    int sel_count = 0; for (int i = 0; i < d->count; i++) if (d->selected[i]) sel_count++;
    children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    char footer[256]; snprintf(footer, sizeof(footer), "Selected: %d/%d  Space:toggle  Enter:confirm  Esc:cancel", sel_count, d->count);
    children[idx].text.content = arena_strdup(g_session_arena, footer); children[idx].style_class = "text"; children[idx].state = "muted"; idx++;
    out->type = RNODE_CONTAINER; out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = children; out->container.child_count = idx;
}

static EventResult multiselect_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    MultiselectData *d = (MultiselectData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_UP: if (d->cursor > 0) { d->cursor--; d->base.dirty = true; } return event_result_handled();
        case KEY_DOWN: if (d->cursor + 1 < d->filtered_count) { d->cursor++; d->base.dirty = true; } return event_result_handled();
        case KEY_CHAR:
            if (ev->ch == ' ') { if (d->filtered_count > 0 && d->cursor < d->filtered_count) { int orig = d->filtered[d->cursor]; int sc = 0; for (int i = 0; i < d->count; i++) if (d->selected[i]) sc++; if (d->selected[orig]) { if (sc > d->min_sel) d->selected[orig] = false; } else { if (sc < d->max_sel) d->selected[orig] = true; } d->base.dirty = true; } }
            else { int len = strlen(d->query); d->query = realloc(d->query, len + 2); d->query[len] = ev->ch; d->query[len+1] = '\0'; d->cursor = 0; d->base.dirty = true; }
            return event_result_handled();
        case KEY_BACKSPACE: if (strlen(d->query) > 0) { d->query[strlen(d->query)-1] = '\0'; d->cursor = 0; d->base.dirty = true; } return event_result_handled();
        case KEY_ENTER: { int sc = 0; for (int i = 0; i < d->count; i++) if (d->selected[i]) sc++; if (sc >= d->min_sel && sc <= d->max_sel) { cJSON *arr = cJSON_CreateArray(); for (int i = 0; i < d->count; i++) if (d->selected[i]) cJSON_AddItemToArray(arr, cJSON_CreateString(d->choices[i])); return event_result_response((WidgetResponse){ .result = arr, .cancelled = false }); } return event_result_handled(); }
        default: return event_result_unhandled();
    }
}

static void multiselect_destroy(Widget *self) { MultiselectData *d = (MultiselectData *)(self + 1); free(d->title); free(d->message); free(d->query); free(d->placeholder); for (int i = 0; i < d->count; i++) free(d->choices[i]); free(d->choices); free(d->selected); free(d->filtered); }

Widget *multiselect_widget_new(const char *title, const char *message, char **choices, int count,
                                const char *placeholder, int min_sel, int max_sel) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(MultiselectData));
    MultiselectData *d = (MultiselectData *)(w + 1);
    d->base.dirty = true;
    d->base.accepts_text_input = true;
    d->title = title ? strdup(title) : NULL;
    d->message = message ? strdup(message) : NULL;
    d->choices = malloc(count * sizeof(char *));
    d->count = count;
    for (int i = 0; i < count; i++) d->choices[i] = strdup(choices[i]);
    d->query = strdup("");
    d->cursor = 0;
    d->min_sel = min_sel;
    d->max_sel = max_sel > 0 ? max_sel : count;
    d->placeholder = placeholder ? strdup(placeholder) : strdup("Type to filter...");
    d->selected = calloc(count, sizeof(bool));
    d->filtered = NULL; d->filtered_count = 0;
    w->vtable.render = multiselect_render;
    w->vtable.handle_event = multiselect_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = multiselect_destroy;
    return w;
}