#include <stdio.h>
#include "yesno.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *title;
    char *message;
    bool selected_yes;
    bool dirty;
} YesNoData;

static void yesno_render(Widget *self, Rect area, RenderTree *out) {
    YesNoData *d = (YesNoData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = 50, box_h = 10;
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    RenderTree *children = calloc(5 + 1, sizeof(RenderTree));
    int idx = 0;
    if (d->title && strlen(d->title)) {
        RenderTree *t = &children[idx++];
        t->type = RNODE_TEXT;
        t->rect = rect_new(1, 0, box_w - 2, 1);
        t->text.content = strdup(d->title);
        t->text.align = ALIGN_CENTER;
        t->text.style = textstyle_selected();
    }
    if (d->message && strlen(d->message)) {
        RenderTree *t = &children[idx++];
        t->type = RNODE_TEXT;
        t->rect = rect_new(1, 2, box_w - 2, 2);
        t->text.content = strdup(d->message);
        t->text.align = ALIGN_CENTER;
        t->text.style = textstyle_normal();
    }
    RenderTree *yes = &children[idx++];
    yes->type = RNODE_TEXT;
    yes->rect = rect_new((box_w - 20) / 2, 5, 8, 1);
    yes->text.content = strdup("[ Yes ]");
    yes->text.align = ALIGN_CENTER;
    yes->text.style = d->selected_yes ? textstyle_selected() : textstyle_muted();

    RenderTree *no = &children[idx++];
    no->type = RNODE_TEXT;
    no->rect = rect_new((box_w - 20) / 2 + 10, 5, 8, 1);
    no->text.content = strdup("[ No ]");
    no->text.align = ALIGN_CENTER;
    no->text.style = d->selected_yes ? textstyle_muted() : textstyle_selected();

    RenderTree *footer = &children[idx++];
    footer->type = RNODE_TEXT;
    footer->rect = rect_new(1, box_h - 2, box_w - 2, 1);
    footer->text.content = strdup("h/l:choose  Enter:confirm  y/n:quick  Esc:cancel");
    footer->text.align = ALIGN_CENTER;
    footer->text.style = textstyle_muted();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = idx;
}

static EventResult yesno_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    YesNoData *d = (YesNoData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_LEFT:
        case KEY_RIGHT:
        case KEY_TAB:
            d->selected_yes = !d->selected_yes;
            d->dirty = true;
            return event_result_handled();
        case KEY_ENTER:
            return event_result_response((WidgetResponse){ .result = cJSON_CreateBool(d->selected_yes ? 1 : 0), .cancelled = false, .error = NULL });
        case KEY_CHAR:
            if (ev->ch == 'y' || ev->ch == 'Y')
                return event_result_response((WidgetResponse){ .result = cJSON_CreateBool(1), .cancelled = false, .error = NULL });
            if (ev->ch == 'n' || ev->ch == 'N')
                return event_result_response((WidgetResponse){ .result = cJSON_CreateBool(0), .cancelled = false, .error = NULL });
            return event_result_unhandled();
        default:
            return event_result_unhandled();
    }
}

static bool yesno_is_dirty(Widget *self) { return ((YesNoData *)(self + 1))->dirty; }
static void yesno_clear_dirty(Widget *self) { ((YesNoData *)(self + 1))->dirty = false; }
static void yesno_destroy(Widget *self) {
    YesNoData *d = (YesNoData *)(self + 1);
    free(d->title);
    free(d->message);
}

Widget *yesno_widget_new(const char *title, const char *message, bool default_yes) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(YesNoData));
    w->vtable.render = yesno_render;
    w->vtable.handle_event = yesno_handle_event;
    w->vtable.is_dirty = yesno_is_dirty;
    w->vtable.clear_dirty = yesno_clear_dirty;
    w->vtable.destroy = yesno_destroy;
    YesNoData *d = (YesNoData *)(w + 1);
    d->title = title ? strdup(title) : NULL;
    d->message = message ? strdup(message) : NULL;
    d->selected_yes = default_yes;
    d->dirty = true;
    return w;
}