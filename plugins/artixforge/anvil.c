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
    char ***action_keys;
    char ***action_descs;
    int *action_counts;
    int cat_idx;
    int action_idx;
    int mode;
    char *confirm_key;
    bool dirty;
} AnvilData;

static void anvil_render(Widget *self, Rect area, RenderTree *out) {
    AnvilData *d = (AnvilData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.85f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.90f);
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
        children[idx].text.content = strdup("No actions available.");
        children[idx].text.align = ALIGN_LEFT;
        children[idx].text.style = textstyle_normal();
        idx++;
    } else {
        int left_w = box_w * 30 / 100;
        int right_x = left_w + 2;
        int right_w = box_w - right_x - 1;

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

        int ac = d->action_counts[d->cat_idx];
        children[idx].type = RNODE_LIST;
        children[idx].rect = rect_new(right_x, 1, right_w, box_h - 3);
        children[idx].list.item_count = ac;
        children[idx].list.items = malloc(ac * sizeof(ListItem));
        for (int i = 0; i < ac; i++) {
            char label[512];
            snprintf(label, sizeof(label), "%s %s", (i == d->action_idx && d->mode == 0) ? ">" : "  ", d->action_descs[d->cat_idx][i]);
            children[idx].list.items[i] = listitem_new(label);
        }
        children[idx].list.selected = d->action_idx;
        children[idx].list.highlight = textstyle_selected();
        idx++;
    }

    if (d->mode == 0) {
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
        children[idx].text.content = strdup("Up/Down:actions  Left/Right:categories  Enter:execute  Esc:cancel");
        children[idx].text.align = ALIGN_CENTER;
        children[idx].text.style = textstyle_muted();
    } else {
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, box_h - 3, box_w - 2, 1);
        char buf[256];
        snprintf(buf, sizeof(buf), "Execute '%s'?", d->confirm_key);
        children[idx].text.content = strdup(buf);
        children[idx].text.align = ALIGN_CENTER;
        children[idx].text.style = textstyle_accent();
        idx++;
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
        children[idx].text.content = strdup("[Y]es  [N]o");
        children[idx].text.align = ALIGN_CENTER;
        children[idx].text.style = textstyle_accent();
    }
    idx++;

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = idx;
}

static EventResult anvil_handle(Widget *self, Event *ev, Backend *backend) {
    AnvilData *d = (AnvilData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();

    if (d->mode == 1) {
        if (ev->code == KEY_CHAR && (ev->ch == 'y' || ev->ch == 'Y'))
            return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->confirm_key), .cancelled = false, .error = NULL });
        d->mode = 0; d->dirty = true;
        return event_result_handled();
    }

    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_UP:
            d->action_idx = d->action_idx > 0 ? d->action_idx - 1 : 0;
            d->dirty = true; return event_result_handled();
        case KEY_DOWN:
            if (d->cat_count > 0 && d->action_idx + 1 < d->action_counts[d->cat_idx]) d->action_idx++;
            d->dirty = true; return event_result_handled();
        case KEY_LEFT:
            d->cat_idx = d->cat_idx > 0 ? d->cat_idx - 1 : 0;
            d->action_idx = 0; d->dirty = true; return event_result_handled();
        case KEY_RIGHT:
            if (d->cat_idx + 1 < d->cat_count) d->cat_idx++;
            d->action_idx = 0; d->dirty = true; return event_result_handled();
        case KEY_ENTER:
            if (d->cat_count > 0 && d->action_idx < d->action_counts[d->cat_idx]) {
                d->confirm_key = strdup(d->action_keys[d->cat_idx][d->action_idx]);
                d->mode = 1; d->dirty = true;
            }
            return event_result_handled();
        default: return event_result_unhandled();
    }
}

static bool anvil_is_dirty(Widget *self) { return ((AnvilData *)(self + 1))->dirty; }
static void anvil_clear_dirty(Widget *self) { ((AnvilData *)(self + 1))->dirty = false; }
static void anvil_destroy(Widget *self) {
    AnvilData *d = (AnvilData *)(self + 1);
    free(d->title); free(d->confirm_key);
    for (int i = 0; i < d->cat_count; i++) {
        free(d->cat_names[i]);
        for (int j = 0; j < d->action_counts[i]; j++) {
            free(d->action_keys[i][j]); free(d->action_descs[i][j]);
        }
        free(d->action_keys[i]); free(d->action_descs[i]);
    }
    free(d->cat_names); free(d->action_keys); free(d->action_descs); free(d->action_counts);
}

Widget *anvil_factory(const WidgetRequest *req) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(AnvilData));
    w->vtable.render = anvil_render;
    w->vtable.handle_event = anvil_handle;
    w->vtable.is_dirty = anvil_is_dirty;
    w->vtable.clear_dirty = anvil_clear_dirty;
    w->vtable.destroy = anvil_destroy;
    AnvilData *d = (AnvilData *)(w + 1);
    d->title = strdup(cJSON_GetObjectItem(req->params, "title")->valuestring ?: "Anvil");
    d->cat_count = 0; d->cat_names = NULL;
    d->action_keys = NULL; d->action_descs = NULL; d->action_counts = NULL;
    d->cat_idx = 0; d->action_idx = 0; d->mode = 0; d->confirm_key = NULL; d->dirty = true;
    cJSON *cats = cJSON_GetObjectItem(req->params, "categories");
    if (cats && cats->type == cJSON_Array) {
        d->cat_count = cJSON_GetArraySize(cats);
        d->cat_names = malloc(d->cat_count * sizeof(char *));
        d->action_keys = malloc(d->cat_count * sizeof(char **));
        d->action_descs = malloc(d->cat_count * sizeof(char **));
        d->action_counts = calloc(d->cat_count, sizeof(int));
        int ci = 0;
        cJSON *cat;
        cJSON_ArrayForEach(cat, cats) {
            d->cat_names[ci] = strdup(cJSON_GetObjectItem(cat, "category")->valuestring ?: "");
            cJSON *actions = cJSON_GetObjectItem(cat, "actions");
            int ac = actions ? cJSON_GetArraySize(actions) : 0;
            d->action_counts[ci] = ac;
            d->action_keys[ci] = malloc(ac * sizeof(char *));
            d->action_descs[ci] = malloc(ac * sizeof(char *));
            int ai = 0;
            cJSON *act;
            cJSON_ArrayForEach(act, actions) {
                d->action_keys[ci][ai] = strdup(cJSON_GetObjectItem(act, "key")->valuestring ?: "");
                d->action_descs[ci][ai] = strdup(cJSON_GetObjectItem(act, "description")->valuestring ?: "");
                ai++;
            }
            ci++;
        }
    }
    return w;
}