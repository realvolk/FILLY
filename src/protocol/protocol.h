#pragma once
#include "cJSON.h"
#include <stdbool.h>

typedef struct {
    char *widget;
    cJSON *params;
    int step;
    int total;
    char *session_id;
    char *tty;
    bool relay;
    bool headless;
    char *anchor;
    int x, y;
    char *relative_to;
    int dx, dy;
    int z_index;
    char *overflow;
    char *tooltip;
    int tab_index;
    bool draggable;
} WidgetRequest;

typedef struct {
    cJSON *result;
    bool cancelled;
    char *error;
} WidgetResponse;

WidgetRequest *widget_request_parse(const char *json_str);
void widget_request_free(WidgetRequest *req);
char *widget_response_to_json(WidgetResponse *resp);
void widget_response_free(WidgetResponse *resp);