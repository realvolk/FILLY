#include "rich_text.h"
#include "core/widget_base.h"
#include <stdlib.h>
#include <string.h>

typedef struct { WidgetBase base; char *content; } RichTextData;

static void rich_text_render(Widget *self, Rect area, RenderTree *out) {
    RichTextData *d = (RichTextData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->type = RNODE_TEXT;
    out->rect = area;
    out->text.content = d->content;
    out->style_class = "text";
}

static EventResult rich_text_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)self;
    (void)backend;
    if (ev->type == EVENT_KEY)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false });
    return event_result_unhandled();
}

static void rich_text_destroy(Widget *self) {
    free(((RichTextData *)(self + 1))->content);
}

Widget *rich_text_widget_new(const char *content) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(RichTextData));
    RichTextData *d = (RichTextData *)(w + 1);
    d->base.dirty = true;
    d->content = strdup(content);
    w->vtable.render = rich_text_render;
    w->vtable.handle_event = rich_text_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = rich_text_destroy;
    return w;
}