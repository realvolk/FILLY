#include "../../filly-core/widget.h"
#include "../../filly-core/render.h"
#include "../../filly-protocol/protocol.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
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
    bool dirty;
} IsoData;

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
    int box_w = (int)(area.w * 0.85f), box_h = (int)(area.h * 0.90f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    RenderTree *children = calloc(4 + 1, sizeof(RenderTree));
    int idx = 0;
    children[idx].type = RNODE_TEXT;
    children[idx].rect = rect_new(1, 0, box_w - 2, 1);
    children[idx].text.content = strdup(d->title);
    children[idx].text.align = ALIGN_CENTER;
    children[idx].text.style = textstyle_selected();
    idx++;

    if (d->cat_count == 0) {
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, 1, box_w - 2, 1);
        children[idx].text.content = strdup("No categories configured.");
        children[idx].text.align = ALIGN_LEFT;
        children[idx].text.style = textstyle_normal();
        idx++;
    } else {
        int left_w = box_w * 35 / 100, right_x = left_w + 2, right_w = box_w - right_x - 1;
        children[idx].type = RNODE_LIST;
        children[idx].rect = rect_new(1, 1, left_w, box_h - 3);
        children[idx].list.item_count = d->cat_count;
        children[idx].list.items = malloc(d->cat_count * sizeof(ListItem));
        for (int i = 0; i < d->cat_count; i++) {
            char label[256];
            snprintf(label, sizeof(label), "%s %s", i == d->cat_idx ? ">" : " ", d->cat_names[i]);
            children[idx].list.items[i] = listitem_new(label);
        }
        children[idx].list.selected = d->cat_idx;
        children[idx].list.highlight = textstyle_selected();
        idx++;

        int ic = d->item_counts[d->cat_idx];
        children[idx].type = RNODE_LIST;
        children[idx].rect = rect_new(right_x, 1, right_w, box_h - 3);
        children[idx].list.item_count = ic;
        children[idx].list.items = malloc(ic * sizeof(ListItem));
        for (int i = 0; i < ic; i++) {
            char *val = iso_get(d, d->item_ids[d->cat_idx][i]);
            char label[512];
            snprintf(label, sizeof(label), "%s %s: %s", i == d->item_idx ? ">" : "  ", d->item_ids[d->cat_idx][i], val ? val : "(none)");
            children[idx].list.items[i] = listitem_new(label);
        }
        children[idx].list.selected = d->item_idx;
        children[idx].list.highlight = textstyle_selected();
        idx++;
    }

    children[idx].type = RNODE_TEXT;
    children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[idx].text.content = strdup(d->mode == 1 ? "[Y]es  [N]o" : "Up/Down:items  Left/Right:categories  Enter:edit  F1:Build  Esc:cancel");
    children[idx].text.align = ALIGN_CENTER;
    children[idx].text.style = textstyle_muted();
    idx++;

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = idx;
}

static EventResult iso_handle(Widget *self, Event *ev, Backend *backend) {
    IsoData *d = (IsoData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    if (d->mode == 1) {
        if (ev->code == KEY_CHAR && (ev->ch == 'y' || ev->ch == 'Y')) {
            cJSON *obj = cJSON_CreateObject();
            for (int i = 0; i < d->val_count; i++) cJSON_AddStringToObject(obj, d->keys[i], d->vals[i]);
            return event_result_response((WidgetResponse){ .result = obj, .cancelled = false, .error = NULL });
        }
        d->mode = 0; d->dirty = true; return event_result_handled();
    }
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_UP: d->item_idx = d->item_idx > 0 ? d->item_idx - 1 : 0; d->dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->cat_count > 0 && d->item_idx + 1 < d->item_counts[d->cat_idx]) d->item_idx++; d->dirty = true; return event_result_handled();
        case KEY_LEFT: d->cat_idx = d->cat_idx > 0 ? d->cat_idx - 1 : 0; d->item_idx = 0; d->dirty = true; return event_result_handled();
        case KEY_RIGHT: if (d->cat_idx + 1 < d->cat_count) d->cat_idx++; d->item_idx = 0; d->dirty = true; return event_result_handled();
        case KEY_ENTER:
            if (d->cat_count > 0 && d->item_idx < d->item_counts[d->cat_idx])
                return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->item_ids[d->cat_idx][d->item_idx]), .cancelled = false, .error = NULL });
            return event_result_handled();
        case KEY_F1: d->mode = 1; d->dirty = true; return event_result_handled();
        default: return event_result_unhandled();
    }
}

static bool iso_is_dirty(Widget *self) { return ((IsoData *)(self + 1))->dirty; }
static void iso_clear_dirty(Widget *self) { ((IsoData *)(self + 1))->dirty = false; }
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
    w->vtable.render = iso_render; w->vtable.handle_event = iso_handle;
    w->vtable.is_dirty = iso_is_dirty; w->vtable.clear_dirty = iso_clear_dirty; w->vtable.destroy = iso_destroy;
    IsoData *d = (IsoData *)(w + 1);
    d->title = strdup(cJSON_GetObjectItem(req->params, "title")->valuestring ?: "ISO Builder");
    d->cat_count = 0; d->cat_names = NULL; d->item_ids = NULL; d->item_values = NULL; d->item_counts = NULL;
    d->cat_idx = 0; d->item_idx = 0; d->keys = NULL; d->vals = NULL; d->val_count = 0;
    d->mode = 0; d->dirty = true;
    cJSON *cats = cJSON_GetObjectItem(req->params, "categories");
    if (cats && cats->type == cJSON_Array) {
        d->cat_count = cJSON_GetArraySize(cats);
        d->cat_names = malloc(d->cat_count * sizeof(char *));
        d->item_ids = malloc(d->cat_count * sizeof(char **));
        d->item_values = malloc(d->cat_count * sizeof(char **));
        d->item_counts = malloc(d->cat_count * sizeof(int));
        int ci = 0; cJSON *cat;
        cJSON_ArrayForEach(cat, cats) {
            d->cat_names[ci] = strdup(cJSON_GetObjectItem(cat, "label")->valuestring ?: "");
            cJSON *items = cJSON_GetObjectItem(cat, "items");
            int ic = items ? cJSON_GetArraySize(items) : 0;
            d->item_counts[ci] = ic;
            d->item_ids[ci] = malloc(ic * sizeof(char *));
            d->item_values[ci] = malloc(ic * sizeof(char *));
            int ii = 0; cJSON *item;
            cJSON_ArrayForEach(item, items) {
                const char *id = cJSON_GetObjectItem(item, "id")->valuestring ?: "";
                const char *val = cJSON_GetObjectItem(item, "value")->valuestring ?: "";
                d->item_ids[ci][ii] = strdup(id); d->item_values[ci][ii] = strdup(val);
                iso_set(d, id, val);
                ii++;
            }
            ci++;
        }
    }
    return w;
}