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
    cJSON_Delete(root);
    return req;
}

void widget_request_free(WidgetRequest *req) {
    if (!req) return;
    free(req->widget);
    if (req->params) cJSON_Delete(req->params);
    free(req->session_id);
    free(req);
}

char *widget_response_to_json(WidgetResponse *resp) {
    cJSON *root = cJSON_CreateObject();
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