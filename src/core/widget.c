#include "widget.h"
#include <stdlib.h>
#include <string.h>

typedef struct RegistryEntry_s {
    char *name;
    WidgetFactory factory;
    bool is_plugin;
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
    e->is_plugin = false;
    e->next = registry;
    registry = e;
}

void widget_registry_register_plugin(const char *name, WidgetFactory factory) {
    RegistryEntry *e = malloc(sizeof(RegistryEntry));
    e->name = strdup(name);
    e->factory = factory;
    e->is_plugin = true;
    e->next = registry;
    registry = e;
}

void widget_registry_clear_plugins(void) {
    RegistryEntry **prev = &registry;
    while (*prev) {
        RegistryEntry *e = *prev;
        if (e->is_plugin) {
            *prev = e->next;
            free(e->name);
            free(e);
        } else {
            prev = &e->next;
        }
    }
}

Widget *widget_registry_create(const WidgetRequest *req) {
    for (RegistryEntry *e = registry; e; e = e->next) {
        if (strcmp(e->name, req->widget) == 0) {
            if (!e->factory) return NULL;
            return e->factory(req);
        }
    }
    return NULL;
}

bool widget_registry_enum(int *idx, const char **name, WidgetFactory *factory) {
    RegistryEntry *e = registry;
    for (int i = 0; i < *idx && e; i++) e = e->next;
    if (!e) return false;
    *name = e->name;
    *factory = e->factory;
    (*idx)++;
    return true;
}

int widget_registry_count(void) {
    int count = 0;
    RegistryEntry *e = registry;
    while (e) { count++; e = e->next; }
    return count;
}

void widget_destroy(Widget *w) {
    if (!w) return;
    if (w->vtable.destroy) w->vtable.destroy(w);
    free(w);
}