#include "../../filly-core/widget.h"
#include "../../filly-core/render.h"
#include "../../filly-protocol/protocol.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
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
    bool dirty;
} RecoveryData;

static void rec_render(Widget *self, Rect area, RenderTree *out) {
    RecoveryData *d = (RecoveryData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.85f), box_h = (int)(area.h * 0.85f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    RenderTree *children = calloc(3 + 1, sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = strdup(d->title);
    children[0].text.align = ALIGN_CENTER;
    children[0].text.style = textstyle_selected();

    children[1].type = RNODE_LIST;
    children[1].rect = rect_new(1, 1, box_w - 2, box_h - 4);
    children[1].list.item_count = d->status_count;
    children[1].list.items = malloc(d->status_count * sizeof(ListItem));
    for (int i = 0; i < d->status_count; i++) {
        char label[512];
        const char *icon = strcmp(d->status_values[i], "ok") == 0 ? "OK" : strcmp(d->status_values[i], "warn") == 0 ? "~ " : "!!";
        snprintf(label, sizeof(label), "%s [%s] %s: %s", i == d->selected ? ">" : " ", icon, d->status_labels[i], d->status_values[i]);
        children[1].list.items[i] = listitem_new(label);
    }
    children[1].list.selected = d->selected;
    children[1].list.highlight = textstyle_selected();

    char footer[512] = {0};
    for (int i = 0; i < d->repair_count; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "F%d:%s  ", i + 1, d->repair_labels[i]);
        strcat(footer, buf);
    }
    strcat(footer, "Up/Down:move  Esc:cancel");
    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = strdup(footer);
    children[2].text.align = ALIGN_CENTER;
    children[2].text.style = textstyle_muted();

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
            return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->confirm_key), .cancelled = false, .error = NULL });
        d->mode = 0; d->dirty = true; return event_result_handled();
    }

    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_UP: if (d->selected > 0) d->selected--; d->dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->selected + 1 < d->status_count) d->selected++; d->dirty = true; return event_result_handled();
        default:
            if (ev->code >= KEY_F1 && ev->code <= KEY_F12) {
                int f = ev->code - KEY_F1;
                if (f < d->repair_count) {
                    d->confirm_key = strdup(d->repair_keys[f]);
                    d->mode = 1; d->dirty = true;
                    return event_result_handled();
                }
            }
            return event_result_unhandled();
    }
}

static bool rec_is_dirty(Widget *self) { return ((RecoveryData *)(self + 1))->dirty; }
static void rec_clear_dirty(Widget *self) { ((RecoveryData *)(self + 1))->dirty = false; }
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
    w->vtable.render = rec_render; w->vtable.handle_event = rec_handle;
    w->vtable.is_dirty = rec_is_dirty; w->vtable.clear_dirty = rec_clear_dirty; w->vtable.destroy = rec_destroy;
    RecoveryData *d = (RecoveryData *)(w + 1);
    cJSON *title_j = cJSON_GetObjectItem(req->params, "title");
    d->title = strdup(title_j && title_j->valuestring ? title_j->valuestring : "Recovery");
    d->status_count = 0; d->status_keys = NULL; d->status_labels = NULL; d->status_values = NULL;
    d->repair_count = 0; d->repair_keys = NULL; d->repair_labels = NULL;
    d->selected = 0; d->mode = 0; d->confirm_key = NULL; d->dirty = true;

    cJSON *status = cJSON_GetObjectItem(req->params, "status");
    if (status && status->type == cJSON_Array) {
        d->status_count = cJSON_GetArraySize(status);
        d->status_keys = malloc(d->status_count * sizeof(char *));
        d->status_labels = malloc(d->status_count * sizeof(char *));
        d->status_values = malloc(d->status_count * sizeof(char *));
        int i = 0; cJSON *s;
        cJSON_ArrayForEach(s, status) {
            cJSON *key_j = cJSON_GetObjectItem(s, "key");
            cJSON *label_j = cJSON_GetObjectItem(s, "label");
            cJSON *stat_j = cJSON_GetObjectItem(s, "status");
            d->status_keys[i] = strdup(key_j && key_j->valuestring ? key_j->valuestring : "");
            d->status_labels[i] = strdup(label_j && label_j->valuestring ? label_j->valuestring : "");
            d->status_values[i] = strdup(stat_j && stat_j->valuestring ? stat_j->valuestring : "ok");
            i++;
        }
    }
    cJSON *repairs = cJSON_GetObjectItem(req->params, "repairs");
    if (repairs && repairs->type == cJSON_Array) {
        d->repair_count = cJSON_GetArraySize(repairs);
        d->repair_keys = malloc(d->repair_count * sizeof(char *));
        d->repair_labels = malloc(d->repair_count * sizeof(char *));
        int i = 0; cJSON *r;
        cJSON_ArrayForEach(r, repairs) {
            cJSON *key_j = cJSON_GetObjectItem(r, "key");
            cJSON *label_j = cJSON_GetObjectItem(r, "label");
            d->repair_keys[i] = strdup(key_j && key_j->valuestring ? key_j->valuestring : "");
            d->repair_labels[i] = strdup(label_j && label_j->valuestring ? label_j->valuestring : "");
            i++;
        }
    }
    return w;
}