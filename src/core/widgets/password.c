#include "password.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>

typedef struct { WidgetBase base; char *title, *message, *text, *placeholder; } PasswordData;
extern Arena *g_session_arena;

static void password_render(Widget *self, Rect area, RenderTree *out) {
    PasswordData *d = (PasswordData *)(self + 1);
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
    children[idx].input.text = arena_strdup(g_session_arena, d->text);
    children[idx].input.placeholder = arena_strdup(g_session_arena, d->placeholder ? d->placeholder : "");
    children[idx].input.masked = true;
    children[idx].style_class = "input"; idx++;
    children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[idx].text.content = "Type + Enter:confirm  Esc:cancel";
    children[idx].style_class = "text"; children[idx].state = "muted"; idx++;
    out->type = RNODE_CONTAINER;
    out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = children; out->container.child_count = idx;
}

static EventResult password_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    PasswordData *d = (PasswordData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_ENTER: return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->text), .cancelled = false });
        case KEY_CHAR: { int len = strlen(d->text); d->text = realloc(d->text, len + 2); d->text[len] = ev->ch; d->text[len+1] = '\0'; d->base.dirty = true; return event_result_handled(); }
        case KEY_BACKSPACE: if (strlen(d->text) > 0) { d->text[strlen(d->text)-1] = '\0'; d->base.dirty = true; } return event_result_handled();
        default: return event_result_unhandled();
    }
}

static void password_destroy(Widget *self) { PasswordData *d = (PasswordData *)(self + 1); free(d->title); free(d->message); free(d->text); free(d->placeholder); }

Widget *password_widget_new(const char *title, const char *message, const char *placeholder) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(PasswordData));
    PasswordData *d = (PasswordData *)(w + 1);
    d->base.dirty = true;
    d->base.accepts_text_input = true;
    d->title = title ? strdup(title) : NULL;
    d->message = message ? strdup(message) : NULL;
    d->text = strdup("");
    d->placeholder = placeholder ? strdup(placeholder) : strdup("");
    w->vtable.render = password_render;
    w->vtable.handle_event = password_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = password_destroy;
    return w;
}