#include <stdio.h>
#include "input.h"
#include <stdlib.h>
#include <string.h>
#include <regex.h>

typedef struct {
    char *title;
    char *message;
    char *text;
    int cursor;
    char *placeholder;
    regex_t *validation;
    bool dirty;
} InputData;

static void input_render(Widget *self, Rect area, RenderTree *out) {
    InputData *d = (InputData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = 60, box_h = 10;
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    out->accessible.role = strdup("dialog");
    out->accessible.label = strdup(d->title ? d->title : "Input");

    RenderTree *children = calloc(4 + 1, sizeof(RenderTree));
    int idx = 0;
    if (d->title && strlen(d->title)) {
        RenderTree *t = &children[idx++];
        t->type = RNODE_TEXT;
        t->rect = rect_new(1, 0, box_w - 2, 1);
        t->text.content = strdup(d->title);
        t->text.align = ALIGN_CENTER;
        t->text.style = textstyle_selected();
        t->accessible.role = strdup("heading");
        t->accessible.label = strdup(d->title);
    }
    int input_y = 2;
    if (d->message && strlen(d->message)) {
        RenderTree *t = &children[idx++];
        t->type = RNODE_TEXT;
        t->rect = rect_new(1, 1, box_w - 2, 2);
        t->text.content = strdup(d->message);
        t->text.align = ALIGN_LEFT;
        t->text.style = textstyle_normal();
        t->accessible.role = strdup("description");
        input_y = 4;
    }
    RenderTree *inp = &children[idx++];
    inp->type = RNODE_INPUT;
    inp->rect = rect_new(1, input_y, box_w - 2, 1);
    inp->input.text = d->text ? strdup(d->text) : strdup("");
    inp->input.cursor = d->cursor;
    inp->input.placeholder = d->placeholder ? strdup(d->placeholder) : strdup("");
    inp->input.masked = false;
    inp->accessible.role = strdup("textbox");
    inp->accessible.label = strdup(d->placeholder ? d->placeholder : "Input field");

    RenderTree *footer = &children[idx++];
    footer->type = RNODE_TEXT;
    footer->rect = rect_new(1, box_h - 2, box_w - 2, 1);
    footer->text.content = strdup("Type + Enter:confirm  Esc:cancel");
    footer->text.align = ALIGN_CENTER;
    footer->text.style = textstyle_muted();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = idx;
}

static EventResult input_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    InputData *d = (InputData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_ENTER:
            if (d->validation) {
                if (regexec(d->validation, d->text, 0, NULL, 0) != 0)
                    return event_result_handled();
            }
            return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->text), .cancelled = false, .error = NULL });
        case KEY_CHAR: {
            int len = strlen(d->text);
            char *new_text = malloc(len + 2);
            strncpy(new_text, d->text, d->cursor);
            new_text[d->cursor] = ev->ch;
            strcpy(new_text + d->cursor + 1, d->text + d->cursor);
            free(d->text);
            d->text = new_text;
            d->cursor++;
            d->dirty = true;
            return event_result_handled();
        }
        case KEY_BACKSPACE:
            if (d->cursor > 0) {
                memmove(d->text + d->cursor - 1, d->text + d->cursor, strlen(d->text + d->cursor) + 1);
                d->cursor--;
                d->dirty = true;
            }
            return event_result_handled();
        case KEY_DELETE:
            if (d->cursor < (int)strlen(d->text)) {
                memmove(d->text + d->cursor, d->text + d->cursor + 1, strlen(d->text + d->cursor));
                d->dirty = true;
            }
            return event_result_handled();
        case KEY_LEFT:
            if (d->cursor > 0) d->cursor--;
            return event_result_handled();
        case KEY_RIGHT:
            if (d->cursor < (int)strlen(d->text)) d->cursor++;
            return event_result_handled();
        case KEY_HOME:
            d->cursor = 0;
            return event_result_handled();
        case KEY_END:
            d->cursor = strlen(d->text);
            return event_result_handled();
        default:
            return event_result_unhandled();
    }
}

static bool input_is_dirty(Widget *self) { return ((InputData *)(self + 1))->dirty; }
static void input_clear_dirty(Widget *self) { ((InputData *)(self + 1))->dirty = false; }
static void input_destroy(Widget *self) {
    InputData *d = (InputData *)(self + 1);
    free(d->title); free(d->message); free(d->text); free(d->placeholder);
    if (d->validation) { regfree(d->validation); free(d->validation); }
}

Widget *input_widget_new(const char *title, const char *message, const char *default_text, const char *placeholder, const char *validation) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(InputData));
    w->vtable.render = input_render;
    w->vtable.handle_event = input_handle_event;
    w->vtable.is_dirty = input_is_dirty;
    w->vtable.clear_dirty = input_clear_dirty;
    w->vtable.destroy = input_destroy;
    InputData *d = (InputData *)(w + 1);
    d->title = title ? strdup(title) : NULL;
    d->message = message ? strdup(message) : NULL;
    d->text = default_text ? strdup(default_text) : strdup("");
    d->cursor = strlen(d->text);
    d->placeholder = placeholder ? strdup(placeholder) : strdup("");
    if (validation) {
        d->validation = malloc(sizeof(regex_t));
        if (regcomp(d->validation, validation, REG_EXTENDED | REG_NOSUB) != 0) {
            free(d->validation);
            d->validation = NULL;
        }
    } else d->validation = NULL;
    d->dirty = true;
    return w;
}