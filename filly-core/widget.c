#include "widget.h"
#include <stdlib.h>
#include <string.h>

typedef struct RegistryEntry_s {
    char *name;
    WidgetFactory factory;
    struct RegistryEntry_s *next;
} RegistryEntry;

static RegistryEntry *registry = NULL;

EventResult event_result_handled(void) {
    EventResult r = { .type = EVENT_RESULT_HANDLED };
    return r;
}

EventResult event_result_response(WidgetResponse resp) {
    EventResult r = { .type = EVENT_RESULT_RESPONSE, .response = resp };
    return r;
}

EventResult event_result_unhandled(void) {
    EventResult r = { .type = EVENT_RESULT_UNHANDLED };
    return r;
}

void widget_registry_register(const char *name, WidgetFactory factory) {
    RegistryEntry *e = malloc(sizeof(RegistryEntry));
    e->name = strdup(name);
    e->factory = factory;
    e->next = registry;
    registry = e;
}

Widget *widget_registry_create(const WidgetRequest *req) {
    for (RegistryEntry *e = registry; e; e = e->next) {
        if (strcmp(e->name, req->widget) == 0)
            return e->factory(req);
    }
    return NULL;
}

void widget_destroy(Widget *w) {
    if (!w) return;
    if (w->vtable.destroy) w->vtable.destroy(w);
    free(w);
}