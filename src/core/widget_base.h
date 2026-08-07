#pragma once
#include "widget.h"
#include "render.h"
#include <stdbool.h>

typedef struct {
    bool dirty;
    bool accepts_text_input;
    Rect render_area;
    int tab_index;
} WidgetBase;

void widget_base_init(Widget *w, void *data, size_t data_size,
                      void (*render)(Widget*, RenderTree*),
                      EventResult (*handle_event)(Widget*, Event*, Backend*),
                      void (*destroy)(Widget*));

bool widget_base_is_dirty(Widget *w);
void widget_base_clear_dirty(Widget *w);