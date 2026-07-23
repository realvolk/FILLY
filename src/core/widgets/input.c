#include "input.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <regex.h>

typedef struct { WidgetBase base; char *title, *message, *text; int cursor; char *placeholder; regex_t *validation; } InputData;
extern Arena *g_session_arena;

static void input_render(Widget *self, Rect area, RenderTree *out) {
    InputData *d = (InputData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = 60, box_h = 10;
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_h > area.h - 2) box_h = area.h - 2;
    int child_count = (d->title ? 1 : 0) + (d->message ? 1 : 0) + 2;
    RenderTree *children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree));
    int idx = 0;
    if (d->title && strlen(d->title)) {
        children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, 0, box_w - 2, 1);
        children[idx].text.content = arena_strdup(g_session_arena, d->title);
        children[idx].style_class = "text"; children[idx].state = "title"; idx++;
    }
    int input_y = d->message ? 4 : 2;
    if (d->message && strlen(d->message)) {
        children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, 1, box_w - 2, 2);
        children[idx].text.content = arena_strdup(g_session_arena, d->message);
        children[idx].style_class = "text"; idx++;
    }
    children[idx].type = RNODE_INPUT; children[idx].rect = rect_new(1, input_y, box_w - 2, 1);
    children[idx].input.text = arena_strdup(g_session_arena, d->text ? d->text : "");
    children[idx].input.cursor = d->cursor;
    children[idx].input.placeholder = arena_strdup(g_session_arena, d->placeholder ? d->placeholder : "");
    children[idx].style_class = "input"; idx++;
    children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[idx].text.content = "Type + Enter:confirm  Esc:cancel";
    children[idx].style_class = "text"; children[idx].state = "muted"; idx++;
    out->type = RNODE_CONTAINER;
    out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = children; out->container.child_count = idx;
}

static EventResult input_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    InputData *d = (InputData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_ENTER:
            if (d->validation && regexec(d->validation, d->text, 0, NULL, 0) != 0) return event_result_handled();
            return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->text), .cancelled = false });
        case KEY_CHAR: {
            int len = strlen(d->text);
            char *new_text = malloc(len + 2);
            memcpy(new_text, d->text, d->cursor);
            new_text[d->cursor] = ev->ch;
            memcpy(new_text + d->cursor + 1, d->text + d->cursor, len - d->cursor);
            new_text[len + 1] = '\0';
            free(d->text);
            d->text = new_text;
            d->cursor++;
            d->base.dirty = true;
            return event_result_handled();
        }
        case KEY_BACKSPACE:
            if (d->cursor > 0) { memmove(d->text + d->cursor - 1, d->text + d->cursor, strlen(d->text + d->cursor) + 1); d->cursor--; d->base.dirty = true; }
            return event_result_handled();
        case KEY_DELETE:
            if (d->cursor < (int)strlen(d->text)) { memmove(d->text + d->cursor, d->text + d->cursor + 1, strlen(d->text + d->cursor)); d->base.dirty = true; }
            return event_result_handled();
        case KEY_LEFT: if (d->cursor > 0) d->cursor--; return event_result_handled();
        case KEY_RIGHT: if (d->cursor < (int)strlen(d->text)) d->cursor++; return event_result_handled();
        case KEY_HOME: d->cursor = 0; return event_result_handled();
        case KEY_END: d->cursor = strlen(d->text); return event_result_handled();
        default: return event_result_unhandled();
    }
}

static void input_destroy(Widget *self) {
    InputData *d = (InputData *)(self + 1);
    free(d->title); free(d->message); free(d->text); free(d->placeholder);
    if (d->validation) { regfree(d->validation); free(d->validation); }
}

Widget *input_widget_new(const char *title, const char *message, const char *default_text, const char *placeholder, const char *validation) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(InputData));
    InputData *d = (InputData *)(w + 1);
    d->base.dirty = true;
    d->base.accepts_text_input = true;
    d->title = title ? strdup(title) : NULL;
    d->message = message ? strdup(message) : NULL;
    d->text = default_text ? strdup(default_text) : strdup("");
    d->cursor = default_text ? strlen(default_text) : 0;
    d->placeholder = placeholder ? strdup(placeholder) : strdup("");
    if (validation) {
        d->validation = malloc(sizeof(regex_t));
        if (regcomp(d->validation, validation, REG_EXTENDED | REG_NOSUB) != 0) { free(d->validation); d->validation = NULL; }
    } else d->validation = NULL;
    w->vtable.render = input_render;
    w->vtable.handle_event = input_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = input_destroy;
    return w;
}