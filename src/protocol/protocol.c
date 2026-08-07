#include "protocol.h"
#include <stdlib.h>
#include <string.h>

WidgetRequest *widget_request_parse(const char *json_str) {
    cJSON *root = cJSON_Parse(json_str);
    if (!root) return NULL;
    WidgetRequest *req = calloc(1, sizeof(WidgetRequest));
    cJSON *w = cJSON_GetObjectItem(root, "widget");
    if (w && w->valuestring) req->widget = strdup(w->valuestring);
    cJSON *p = cJSON_GetObjectItem(root, "params");
    if (p) req->params = cJSON_Duplicate(p, 1);
    cJSON *s = cJSON_GetObjectItem(root, "step");
    if (s) req->step = s->valueint;
    cJSON *t = cJSON_GetObjectItem(root, "total");
    if (t) req->total = t->valueint;
    cJSON *sid = cJSON_GetObjectItem(root, "session_id");
    if (sid && sid->valuestring) req->session_id = strdup(sid->valuestring);
    cJSON *tty = cJSON_GetObjectItem(root, "tty");
    if (tty && tty->valuestring) req->tty = strdup(tty->valuestring);
    cJSON *relay = cJSON_GetObjectItem(root, "relay");
    if (relay) req->relay = relay->valueint ? true : false;
    cJSON *headless = cJSON_GetObjectItem(root, "headless");
    if (headless) req->headless = headless->valueint ? true : false;

    cJSON *anchor = cJSON_GetObjectItem(root, "anchor");
    if (anchor && anchor->valuestring) req->anchor = strdup(anchor->valuestring);
    cJSON *x = cJSON_GetObjectItem(root, "x");
    if (x && x->type == cJSON_Number) req->x = x->valueint;
    cJSON *y = cJSON_GetObjectItem(root, "y");
    if (y && y->type == cJSON_Number) req->y = y->valueint;
    cJSON *relative_to = cJSON_GetObjectItem(root, "relative_to");
    if (relative_to && relative_to->valuestring) req->relative_to = strdup(relative_to->valuestring);
    cJSON *dx = cJSON_GetObjectItem(root, "dx");
    if (dx && dx->type == cJSON_Number) req->dx = dx->valueint;
    cJSON *dy = cJSON_GetObjectItem(root, "dy");
    if (dy && dy->type == cJSON_Number) req->dy = dy->valueint;
    cJSON *z = cJSON_GetObjectItem(root, "z_index");
    if (z && z->type == cJSON_Number) req->z_index = z->valueint;
    cJSON *overflow = cJSON_GetObjectItem(root, "overflow");
    if (overflow && overflow->valuestring) req->overflow = strdup(overflow->valuestring);
    cJSON *tooltip = cJSON_GetObjectItem(root, "tooltip");
    if (tooltip && tooltip->valuestring) req->tooltip = strdup(tooltip->valuestring);
    cJSON *tab = cJSON_GetObjectItem(root, "tab_index");
    if (tab && tab->type == cJSON_Number) req->tab_index = tab->valueint;
    cJSON *drag = cJSON_GetObjectItem(root, "draggable");
    if (drag) req->draggable = drag->valueint ? true : false;

    cJSON_Delete(root);
    return req;
}

void widget_request_free(WidgetRequest *req) {
    if (!req) return;
    free(req->widget);
    if (req->params) cJSON_Delete(req->params);
    free(req->session_id);
    free(req->tty);
    free(req->anchor);
    free(req->relative_to);
    free(req->overflow);
    free(req->tooltip);
    free(req);
}

char *widget_response_to_json(WidgetResponse *resp) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "response");
    if (resp->result) cJSON_AddItemToObject(root, "result", cJSON_Duplicate(resp->result, 1));
    else cJSON_AddNullToObject(root, "result");
    cJSON_AddBoolToObject(root, "cancelled", resp->cancelled ? 1 : 0);
    if (resp->error) cJSON_AddStringToObject(root, "error", resp->error);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

void widget_response_free(WidgetResponse *resp) {
    if (!resp) return;
    if (resp->result) cJSON_Delete(resp->result);
    free(resp->error);
}