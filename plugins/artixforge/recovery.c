#include "core/widget.h"
#include "core/render.h"
#include "core/widget_base.h"
#include "core/session.h"
#include "protocol/protocol.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    WidgetBase base;
    char *title;
    char **status_keys;
    char **status_labels;
    char **status_values;
    int status_count;
    char **repair_keys;
    char **repair_labels;
    int repair_count;
    int selected;
    int mode;
    char *confirm_key;
} RecoveryData;

extern Arena *g_session_arena;

static void rec_render(Widget *self, Rect area, RenderTree *out) {
    RecoveryData *d = (RecoveryData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = (int)(area.w * 0.85f), box_h = (int)(area.h * 0.85f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    RenderTree *children = arena_alloc(g_session_arena, 3 * sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = arena_strdup(g_session_arena, d->title);
    children[0].style_class = "text"; children[0].state = "title";

    children[1].type = RNODE_LIST;
    children[1].rect = rect_new(1, 1, box_w - 2, box_h - 4);
    children[1].list.item_count = d->status_count;
    children[1].list.selected = d->selected;
    children[1].list.items = arena_alloc(g_session_arena, d->status_count * sizeof(ListItem));
    for (int i = 0; i < d->status_count; i++) {
        char label[512];
        const char *icon = strcmp(d->status_values[i], "ok") == 0 ? "OK" : strcmp(d->status_values[i], "warn") == 0 ? "~ " : "!!";
        snprintf(label, sizeof(label), "%s [%s] %s: %s", i == d->selected ? ">" : " ", icon, d->status_labels[i], d->status_values[i]);
        children[1].list.items[i].label = arena_strdup(g_session_arena, label);
    }
    children[1].style_class = "list";

    char footer[512] = {0};
    for (int i = 0; i < d->repair_count; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "F%d:%s  ", i + 1, d->repair_labels[i]);
        strcat(footer, buf);
    }
    strcat(footer, "Up/Down:move  Esc:cancel");
    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = arena_strdup(g_session_arena, footer);
    children[2].style_class = "text"; children[2].state = "muted";

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 3;
}

static EventResult rec_handle(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    RecoveryData *d = (RecoveryData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();

    if (d->mode == 1) {
        if (ev->code == KEY_CHAR && (ev->ch == 'y' || ev->ch == 'Y'))
            return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->confirm_key), .cancelled = false });
        d->mode = 0; d->base.dirty = true; return event_result_handled();
    }

    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_UP: if (d->selected > 0) d->selected--; d->base.dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->selected + 1 < d->status_count) d->selected++; d->base.dirty = true; return event_result_handled();
        default:
            if (ev->code >= KEY_F1 && ev->code <= KEY_F12) {
                int f = ev->code - KEY_F1;
                if (f < d->repair_count) {
                    d->confirm_key = strdup(d->repair_keys[f]);
                    d->mode = 1; d->base.dirty = true;
                    return event_result_handled();
                }
            }
            return event_result_unhandled();
    }
}

static void rec_destroy(Widget *self) {
    RecoveryData *d = (RecoveryData *)(self + 1);
    free(d->title); free(d->confirm_key);
    for (int i = 0; i < d->status_count; i++) { free(d->status_keys[i]); free(d->status_labels[i]); free(d->status_values[i]); }
    free(d->status_keys); free(d->status_labels); free(d->status_values);
    for (int i = 0; i < d->repair_count; i++) { free(d->repair_keys[i]); free(d->repair_labels[i]); }
    free(d->repair_keys); free(d->repair_labels);
}

Widget *recovery_factory(const WidgetRequest *req) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(RecoveryData));
    RecoveryData data;
    memset(&data, 0, sizeof(data));
    cJSON *title_j = cJSON_GetObjectItem(req->params, "title");
    data.title = strdup(title_j && title_j->valuestring ? title_j->valuestring : "Recovery");

    cJSON *status = cJSON_GetObjectItem(req->params, "status");
    if (status && status->type == cJSON_Array) {
        data.status_count = cJSON_GetArraySize(status);
        data.status_keys = malloc(data.status_count * sizeof(char *));
        data.status_labels = malloc(data.status_count * sizeof(char *));
        data.status_values = malloc(data.status_count * sizeof(char *));
        int i = 0; cJSON *s;
        cJSON_ArrayForEach(s, status) {
            cJSON *key_j = cJSON_GetObjectItem(s, "key");
            cJSON *label_j = cJSON_GetObjectItem(s, "label");
            cJSON *stat_j = cJSON_GetObjectItem(s, "status");
            data.status_keys[i] = strdup(key_j && key_j->valuestring ? key_j->valuestring : "");
            data.status_labels[i] = strdup(label_j && label_j->valuestring ? label_j->valuestring : "");
            data.status_values[i] = strdup(stat_j && stat_j->valuestring ? stat_j->valuestring : "ok");
            i++;
        }
    }
    cJSON *repairs = cJSON_GetObjectItem(req->params, "repairs");
    if (repairs && repairs->type == cJSON_Array) {
        data.repair_count = cJSON_GetArraySize(repairs);
        data.repair_keys = malloc(data.repair_count * sizeof(char *));
        data.repair_labels = malloc(data.repair_count * sizeof(char *));
        int i = 0; cJSON *r;
        cJSON_ArrayForEach(r, repairs) {
            cJSON *key_j = cJSON_GetObjectItem(r, "key");
            cJSON *label_j = cJSON_GetObjectItem(r, "label");
            data.repair_keys[i] = strdup(key_j && key_j->valuestring ? key_j->valuestring : "");
            data.repair_labels[i] = strdup(label_j && label_j->valuestring ? label_j->valuestring : "");
            i++;
        }
    }
    widget_base_init(w, &data, sizeof(RecoveryData), rec_render, rec_handle, rec_destroy);
    return w;
}