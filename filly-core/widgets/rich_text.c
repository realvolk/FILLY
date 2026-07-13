#include <stdio.h>
#include "rich_text.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *content;
    bool dirty;
} RichTextData;

static void rich_text_render(Widget *self, Rect area, RenderTree *out) {
    RichTextData *d = (RichTextData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->type = RNODE_TEXT;
    out->rect = area;
    out->text.content = strdup(d->content);
    out->text.align = ALIGN_LEFT;
    out->text.style = textstyle_normal();
}

static EventResult rich_text_handle_event(Widget *self, Event *ev, Backend *backend) { return event_result_unhandled(); }
static bool rich_text_is_dirty(Widget *self) { return ((RichTextData *)(self + 1))->dirty; }
static void rich_text_clear_dirty(Widget *self) { ((RichTextData *)(self + 1))->dirty = false; }
static void rich_text_destroy(Widget *self) { free(((RichTextData *)(self + 1))->content); }

Widget *rich_text_widget_new(const char *content) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(RichTextData));
    w->vtable.render = rich_text_render;
    w->vtable.handle_event = rich_text_handle_event;
    w->vtable.is_dirty = rich_text_is_dirty;
    w->vtable.clear_dirty = rich_text_clear_dirty;
    w->vtable.destroy = rich_text_destroy;
    RichTextData *d = (RichTextData *)(w + 1);
    d->content = strdup(content);
    d->dirty = true;
    return w;
}