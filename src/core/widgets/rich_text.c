#include "rich_text.h"
#include "core/widget_base.h"
#include <stdlib.h>
#include <string.h>

typedef struct { WidgetBase base; char *content; } RichTextData;

static void rich_text_render(Widget *self, RenderTree *out) {
    RichTextData *d = (RichTextData *)(self + 1);
    WidgetBase *base = (WidgetBase *)(self + 1);
    Rect area = base->render_area;
    memset(out, 0, sizeof(*out));
    out->type = RNODE_RICH_TEXT;
    out->rect = area;
    out->u.rich_text.spans = d->content;
    out->style_class = "rich_text";
    out->accessible.role = "text";
    out->accessible.label = d->content;
    out->tab_index = base->tab_index >= 0 ? base->tab_index : -1;
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
    d->base.tab_index = -1;
    d->content = strdup(content);
    w->vtable.render = rich_text_render;
    w->vtable.handle_event = rich_text_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = rich_text_destroy;
    return w;
}