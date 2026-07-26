#pragma once
#include "backend.h"
#include "render.h"
#include "event.h"
#include "protocol/protocol.h"

typedef struct Widget_s Widget;
typedef Widget *(*WidgetFactory)(const WidgetRequest *req);

typedef enum { EVENT_RESULT_HANDLED, EVENT_RESULT_RESPONSE, EVENT_RESULT_UNHANDLED } EventResultType;
typedef struct { EventResultType type; WidgetResponse response; } EventResult;

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

typedef enum { P_STR, P_INT, P_BOOL, P_JSON, P_STRS } ParamType;
typedef struct { const char *name; ParamType type; const char *def_str; int def_int; } ParamDesc;

EventResult event_result_handled(void);
EventResult event_result_response(WidgetResponse resp);
EventResult event_result_unhandled(void);
void widget_registry_register(const char *name, WidgetFactory factory);
void widget_registry_register_plugin(const char *name, WidgetFactory factory);
void widget_registry_clear_plugins(void);
Widget *widget_registry_create(const WidgetRequest *req);
void widget_destroy(Widget *w);
bool widget_registry_enum(int *idx, const char **name, WidgetFactory *factory);
int widget_registry_count(void);
const ParamDesc *widget_get_params(const char *widget_type, int *count);