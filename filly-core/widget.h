#include "backend.h"
#pragma once
#include "render.h"
#include "event.h"
#include "../filly-protocol/protocol.h"

typedef struct Widget_s Widget;
typedef Widget *(*WidgetFactory)(const WidgetRequest *req);

typedef enum {
    EVENT_RESULT_HANDLED,
    EVENT_RESULT_RESPONSE,
    EVENT_RESULT_UNHANDLED
} EventResultType;

typedef struct {
    EventResultType type;
    WidgetResponse response;
} EventResult;

typedef struct {
    void (*render)(Widget *self, Rect area, RenderTree *out);
    EventResult (*handle_event)(Widget *self, Event *event, Backend *backend);
    bool (*is_dirty)(Widget *self);
    void (*clear_dirty)(Widget *self);
    void (*destroy)(Widget *self);
} WidgetVTable;

struct Widget_s {
    WidgetVTable vtable;
};

EventResult event_result_handled(void);
EventResult event_result_response(WidgetResponse resp);
EventResult event_result_unhandled(void);
void widget_registry_register(const char *name, WidgetFactory factory);
Widget *widget_registry_create(const WidgetRequest *req);
void widget_destroy(Widget *w);