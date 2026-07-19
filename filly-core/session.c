#include <string.h>
#include <stdio.h>
#include "session.h"
#include "render.h"
#include <stdlib.h>

WidgetResponse session_run(Widget *w, Backend *backend) {
    if (!w || !backend || !backend->vtable) {
        WidgetResponse err = { .result = NULL, .cancelled = true, .error = "Invalid widget or backend" };
        return err;
    }
    if (!w->vtable.render || !w->vtable.handle_event ||
        !w->vtable.is_dirty || !w->vtable.clear_dirty) {
        WidgetResponse err = { .result = NULL, .cancelled = true, .error = "Incomplete widget vtable" };
        return err;
    }

    backend->vtable->setup(backend->data);
    int term_w, term_h;
    backend->vtable->get_size(backend->data, &term_w, &term_h);

    int idle_count = 0;

    while (1) {
        if (w->vtable.is_dirty(w)) {
            RenderTree tree;
            memset(&tree, 0, sizeof(tree));
            w->vtable.render(w, rect_new(0, 0, term_w, term_h), &tree);
            backend->vtable->draw(backend->data, &tree);
            render_tree_free(&tree);
            w->vtable.clear_dirty(w);
        }
        Event ev = backend->vtable->next_event(backend->data);
        if (ev.type == EVENT_NONE) {
            idle_count++;
            if (idle_count > 5000) {
                WidgetResponse timeout = { .result = NULL, .cancelled = false, .error = NULL };
                backend->vtable->teardown(backend->data);
                return timeout;
            }
            continue;
        }
        idle_count = 0;
        EventResult result = w->vtable.handle_event(w, &ev, backend);
        if (result.type == EVENT_RESULT_RESPONSE) {
            backend->vtable->teardown(backend->data);
            return result.response;
        }
    }
}