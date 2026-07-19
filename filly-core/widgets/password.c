#include <stdio.h>
#include "password.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *title;
    char *message;
    char *text;
    char *placeholder;
    bool dirty;
} PasswordData;

static void password_render(Widget *self, Rect area, RenderTree *out) {
    PasswordData *d = (PasswordData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = 60, box_h = 10;
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    out->accessible.role = strdup("dialog");
    out->accessible.label = strdup(d->title ? d->title : "Password");

    RenderTree *children = calloc(3 + 1, sizeof(RenderTree));
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
    inp->input.cursor = 0;
    inp->input.placeholder = d->placeholder ? strdup(d->placeholder) : strdup("");
    inp->input.masked = true;
    inp->accessible.role = strdup("textbox");
    inp->accessible.label = strdup("Password field");

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

static EventResult password_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    PasswordData *d = (PasswordData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_ENTER:
            return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->text), .cancelled = false, .error = NULL });
        case KEY_CHAR: {
            int len = strlen(d->text);
            d->text = realloc(d->text, len + 2);
            d->text[len] = ev->ch;
            d->text[len + 1] = '\0';
            d->dirty = true;
            return event_result_handled();
        }
        case KEY_BACKSPACE:
            if (strlen(d->text) > 0) {
                d->text[strlen(d->text) - 1] = '\0';
                d->dirty = true;
            }
            return event_result_handled();
        default:
            return event_result_unhandled();
    }
}

static bool password_is_dirty(Widget *self) { return ((PasswordData *)(self + 1))->dirty; }
static void password_clear_dirty(Widget *self) { ((PasswordData *)(self + 1))->dirty = false; }
static void password_destroy(Widget *self) {
    PasswordData *d = (PasswordData *)(self + 1);
    free(d->title); free(d->message); free(d->text); free(d->placeholder);
}

Widget *password_widget_new(const char *title, const char *message, const char *placeholder) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(PasswordData));
    w->vtable.render = password_render;
    w->vtable.handle_event = password_handle_event;
    w->vtable.is_dirty = password_is_dirty;
    w->vtable.clear_dirty = password_clear_dirty;
    w->vtable.destroy = password_destroy;
    PasswordData *d = (PasswordData *)(w + 1);
    d->title = title ? strdup(title) : NULL;
    d->message = message ? strdup(message) : NULL;
    d->text = strdup("");
    d->placeholder = placeholder ? strdup(placeholder) : strdup("");
    d->dirty = true;
    return w;
}