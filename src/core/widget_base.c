#include "widget_base.h"
#include <string.h>

void widget_base_init(Widget *w, void *data, size_t data_size,
                      void (*render)(Widget*, Rect, RenderTree*),
                      EventResult (*handle_event)(Widget*, Event*, Backend*),
                      void (*destroy)(Widget*)) {
    w->vtable.render = render;
    w->vtable.handle_event = handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = destroy;
    WidgetBase *base = (WidgetBase *)(w + 1);
    base->dirty = true;
    base->accepts_text_input = false;
    if (data) {
        char *dst = (char *)base + sizeof(WidgetBase);
        char *src = (char *)data + sizeof(WidgetBase);
        memcpy(dst, src, data_size - sizeof(WidgetBase));
    }
}

bool widget_base_is_dirty(Widget *w) {
    return ((WidgetBase *)(w + 1))->dirty;
}

void widget_base_clear_dirty(Widget *w) {
    ((WidgetBase *)(w + 1))->dirty = false;
}