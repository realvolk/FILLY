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
    char **cat_names;
    int cat_count;
    char ***item_ids;
    char ***item_values;
    int *item_counts;
    int cat_idx;
    int item_idx;
    char **keys;
    char **vals;
    int val_count;
    int mode;
} IsoData;

extern Arena *g_session_arena;

static char *iso_get(IsoData *d, const char *k) {
    for (int i = 0; i < d->val_count; i++)
        if (strcmp(d->keys[i], k) == 0) return d->vals[i];
    return NULL;
}

static void iso_set(IsoData *d, const char *k, const char *v) {
    for (int i = 0; i < d->val_count; i++) {
        if (strcmp(d->keys[i], k) == 0) { free(d->vals[i]); d->vals[i] = strdup(v); return; }
    }
    d->val_count++;
    d->keys = realloc(d->keys, d->val_count * sizeof(char *));
    d->vals = realloc(d->vals, d->val_count * sizeof(char *));
    d->keys[d->val_count-1] = strdup(k);
    d->vals[d->val_count-1] = strdup(v);
}

static void iso_render(Widget *self, Rect area, RenderTree *out) {
    IsoData *d = (IsoData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = (int)(area.w * 0.85f), box_h = (int)(area.h * 0.90f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    RenderTree *children = arena_alloc(g_session_arena, 4 * sizeof(RenderTree));
    int idx = 0;
    children[idx].type = RNODE_TEXT;
    children[idx].rect = rect_new(1, 0, box_w - 2, 1);
    children[idx].text.content = arena_strdup(g_session_arena, d->title);
    children[idx].style_class = "text"; children[idx].state = "title";
    idx++;

    if (d->cat_count == 0) {
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, 1, box_w - 2, 1);
        children[idx].text.content = "No categories configured.";
        children[idx].style_class = "text";
        idx++;
    } else {
        int left_w = box_w * 35 / 100, right_x = left_w + 2, right_w = box_w - right_x - 1;
        children[idx].type = RNODE_LIST;
        children[idx].rect = rect_new(1, 1, left_w, box_h - 3);
        children[idx].list.item_count = d->cat_count;
        children[idx].list.selected = d->cat_idx;
        children[idx].list.items = arena_alloc(g_session_arena, d->cat_count * sizeof(ListItem));
        for (int i = 0; i < d->cat_count; i++) {
            char label[256];
            snprintf(label, sizeof(label), "%s %s", i == d->cat_idx ? ">" : " ", d->cat_names[i]);
            children[idx].list.items[i].label = arena_strdup(g_session_arena, label);
        }
        children[idx].style_class = "list";
        idx++;

        int ic = d->item_counts[d->cat_idx];
        children[idx].type = RNODE_LIST;
        children[idx].rect = rect_new(right_x, 1, right_w, box_h - 3);
        children[idx].list.item_count = ic;
        children[idx].list.selected = d->item_idx;
        children[idx].list.items = arena_alloc(g_session_arena, ic * sizeof(ListItem));
        for (int i = 0; i < ic; i++) {
            char *val = iso_get(d, d->item_ids[d->cat_idx][i]);
            char label[512];
            snprintf(label, sizeof(label), "%s %s: %s", i == d->item_idx ? ">" : "  ", d->item_ids[d->cat_idx][i], val ? val : "(none)");
            children[idx].list.items[i].label = arena_strdup(g_session_arena, label);
        }
        children[idx].style_class = "list";
        idx++;
    }

    children[idx].type = RNODE_TEXT;
    children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[idx].text.content = d->mode == 1 ? "[Y]es  [N]o" : "Up/Down:items  Left/Right:categories  Enter:edit  F1:Build  Esc:cancel";
    children[idx].style_class = "text"; children[idx].state = "muted";
    idx++;

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = idx;
}

static EventResult iso_handle(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    IsoData *d = (IsoData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    if (d->mode == 1) {
        if (ev->code == KEY_CHAR && (ev->ch == 'y' || ev->ch == 'Y')) {
            cJSON *obj = cJSON_CreateObject();
            for (int i = 0; i < d->val_count; i++) cJSON_AddStringToObject(obj, d->keys[i], d->vals[i]);
            return event_result_response((WidgetResponse){ .result = obj, .cancelled = false });
        }
        d->mode = 0; d->base.dirty = true; return event_result_handled();
    }
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_UP: d->item_idx = d->item_idx > 0 ? d->item_idx - 1 : 0; d->base.dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->cat_count > 0 && d->item_idx + 1 < d->item_counts[d->cat_idx]) d->item_idx++; d->base.dirty = true; return event_result_handled();
        case KEY_LEFT: d->cat_idx = d->cat_idx > 0 ? d->cat_idx - 1 : 0; d->item_idx = 0; d->base.dirty = true; return event_result_handled();
        case KEY_RIGHT: if (d->cat_idx + 1 < d->cat_count) d->cat_idx++; d->item_idx = 0; d->base.dirty = true; return event_result_handled();
        case KEY_ENTER:
            if (d->cat_count > 0 && d->item_idx < d->item_counts[d->cat_idx])
                return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->item_ids[d->cat_idx][d->item_idx]), .cancelled = false });
            return event_result_handled();
        case KEY_F1: d->mode = 1; d->base.dirty = true; return event_result_handled();
        default: return event_result_unhandled();
    }
}

static void iso_destroy(Widget *self) {
    IsoData *d = (IsoData *)(self + 1);
    free(d->title);
    for (int i = 0; i < d->cat_count; i++) {
        free(d->cat_names[i]);
        for (int j = 0; j < d->item_counts[i]; j++) { free(d->item_ids[i][j]); free(d->item_values[i][j]); }
        free(d->item_ids[i]); free(d->item_values[i]);
    }
    free(d->cat_names); free(d->item_ids); free(d->item_values); free(d->item_counts);
    for (int i = 0; i < d->val_count; i++) { free(d->keys[i]); free(d->vals[i]); }
    free(d->keys); free(d->vals);
}

Widget *iso_factory(const WidgetRequest *req) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(IsoData));
    IsoData data;
    memset(&data, 0, sizeof(data));
    cJSON *title_j = cJSON_GetObjectItem(req->params, "title");
    data.title = strdup(title_j && title_j->valuestring ? title_j->valuestring : "ISO Builder");
    cJSON *cats = cJSON_GetObjectItem(req->params, "categories");
    if (cats && cats->type == cJSON_Array) {
        data.cat_count = cJSON_GetArraySize(cats);
        data.cat_names = malloc(data.cat_count * sizeof(char *));
        data.item_ids = malloc(data.cat_count * sizeof(char **));
        data.item_values = malloc(data.cat_count * sizeof(char **));
        data.item_counts = malloc(data.cat_count * sizeof(int));
        int ci = 0; cJSON *cat;
        cJSON_ArrayForEach(cat, cats) {
            cJSON *cat_label = cJSON_GetObjectItem(cat, "label");
            data.cat_names[ci] = strdup(cat_label && cat_label->valuestring ? cat_label->valuestring : "");
            cJSON *items = cJSON_GetObjectItem(cat, "items");
            int ic = items ? cJSON_GetArraySize(items) : 0;
            data.item_counts[ci] = ic;
            data.item_ids[ci] = malloc(ic * sizeof(char *));
            data.item_values[ci] = malloc(ic * sizeof(char *));
            int ii = 0; cJSON *item;
            cJSON_ArrayForEach(item, items) {
                cJSON *id_j = cJSON_GetObjectItem(item, "id");
                cJSON *val_j = cJSON_GetObjectItem(item, "value");
                data.item_ids[ci][ii] = strdup(id_j && id_j->valuestring ? id_j->valuestring : "");
                data.item_values[ci][ii] = strdup(val_j && val_j->valuestring ? val_j->valuestring : "");
                iso_set(&data, data.item_ids[ci][ii], data.item_values[ci][ii]);
                ii++;
            }
            ci++;
        }
    }
    widget_base_init(w, &data, sizeof(IsoData), iso_render, iso_handle, iso_destroy);
    return w;
}