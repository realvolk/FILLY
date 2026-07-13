#include <string.h>
#include <stdio.h>
#include "session.h"
#include <stdlib.h>

WidgetResponse session_run(Widget *w, Backend *backend) {
    backend->vtable->setup(backend->data);
    int term_w, term_h;
    backend->vtable->get_size(backend->data, &term_w, &term_h);

    while (1) {
        if (w->vtable.is_dirty(w)) {
            RenderTree tree;
            memset(&tree, 0, sizeof(tree));
            w->vtable.render(w, rect_new(0, 0, term_w, term_h), &tree);
            backend->vtable->draw(backend->data, &tree);
            w->vtable.clear_dirty(w);
        }
        Event ev = backend->vtable->next_event(backend->data);
        EventResult result = w->vtable.handle_event(w, &ev, backend);
        if (result.type == EVENT_RESULT_RESPONSE) {
            backend->vtable->teardown(backend->data);
            return result.response;
        }
    }
}