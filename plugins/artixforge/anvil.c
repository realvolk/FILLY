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
    char ***action_keys;
    char ***action_descs;
    int *action_counts;
    int cat_idx;
    int action_idx;
    int mode;
    char *confirm_key;
} AnvilData;

extern Arena *g_session_arena;

static void anvil_render(Widget *self, Rect area, RenderTree *out) {
    AnvilData *d = (AnvilData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = (int)(area.w * 0.85f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.90f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    int child_count = 3 + (d->cat_count > 0 ? 1 : 0);
    if (d->mode == 1) child_count++;
    RenderTree *children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree));
    int idx = 0;

    children[idx].type = RNODE_TEXT;
    children[idx].rect = rect_new(1, 0, box_w - 2, 1);
    children[idx].text.content = arena_strdup(g_session_arena, d->title);
    children[idx].style_class = "text"; children[idx].state = "title";
    idx++;

    if (d->cat_count == 0) {
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, 1, box_w - 2, 1);
        children[idx].text.content = "No actions available.";
        children[idx].style_class = "text";
        idx++;
    } else {
        int left_w = box_w * 30 / 100;
        int right_x = left_w + 2;
        int right_w = box_w - right_x - 1;

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

        int ac = d->action_counts[d->cat_idx];
        children[idx].type = RNODE_LIST;
        children[idx].rect = rect_new(right_x, 1, right_w, box_h - 3);
        children[idx].list.item_count = ac;
        children[idx].list.selected = d->action_idx;
        children[idx].list.items = arena_alloc(g_session_arena, ac * sizeof(ListItem));
        for (int i = 0; i < ac; i++) {
            char label[512];
            snprintf(label, sizeof(label), "%s %s", (i == d->action_idx && d->mode == 0) ? ">" : "  ", d->action_descs[d->cat_idx][i]);
            children[idx].list.items[i].label = arena_strdup(g_session_arena, label);
        }
        children[idx].style_class = "list";
        idx++;
    }

    if (d->mode == 0) {
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
        children[idx].text.content = "Up/Down:actions  Left/Right:categories  Enter:execute  Esc:cancel";
        children[idx].style_class = "text"; children[idx].state = "muted";
    } else {
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, box_h - 3, box_w - 2, 1);
        char buf[256];
        snprintf(buf, sizeof(buf), "Execute '%s'?", d->confirm_key);
        children[idx].text.content = arena_strdup(g_session_arena, buf);
        children[idx].style_class = "text";
        idx++;
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
        children[idx].text.content = "[Y]es  [N]o";
        children[idx].style_class = "text";
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
    (void)backend;
    AnvilData *d = (AnvilData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();

    if (d->mode == 1) {
        if (ev->code == KEY_CHAR && (ev->ch == 'y' || ev->ch == 'Y'))
            return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->confirm_key), .cancelled = false });
        d->mode = 0; d->base.dirty = true;
        return event_result_handled();
    }

    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_UP:
            d->action_idx = d->action_idx > 0 ? d->action_idx - 1 : 0;
            d->base.dirty = true; return event_result_handled();
        case KEY_DOWN:
            if (d->cat_count > 0 && d->action_idx + 1 < d->action_counts[d->cat_idx]) d->action_idx++;
            d->base.dirty = true; return event_result_handled();
        case KEY_LEFT:
            d->cat_idx = d->cat_idx > 0 ? d->cat_idx - 1 : 0;
            d->action_idx = 0; d->base.dirty = true; return event_result_handled();
        case KEY_RIGHT:
            if (d->cat_idx + 1 < d->cat_count) d->cat_idx++;
            d->action_idx = 0; d->base.dirty = true; return event_result_handled();
        case KEY_ENTER:
            if (d->cat_count > 0 && d->action_idx < d->action_counts[d->cat_idx]) {
                d->confirm_key = strdup(d->action_keys[d->cat_idx][d->action_idx]);
                d->mode = 1; d->base.dirty = true;
            }
            return event_result_handled();
        default: return event_result_unhandled();
    }
}

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
    AnvilData data;
    memset(&data, 0, sizeof(data));
    cJSON *title_j = cJSON_GetObjectItem(req->params, "title");
    data.title = strdup(title_j && title_j->valuestring ? title_j->valuestring : "Anvil");
    cJSON *cats = cJSON_GetObjectItem(req->params, "categories");
    if (cats && cats->type == cJSON_Array) {
        data.cat_count = cJSON_GetArraySize(cats);
        data.cat_names = malloc(data.cat_count * sizeof(char *));
        data.action_keys = malloc(data.cat_count * sizeof(char **));
        data.action_descs = malloc(data.cat_count * sizeof(char **));
        data.action_counts = calloc(data.cat_count, sizeof(int));
        int ci = 0;
        cJSON *cat;
        cJSON_ArrayForEach(cat, cats) {
            cJSON *cat_label = cJSON_GetObjectItem(cat, "category");
            data.cat_names[ci] = strdup(cat_label && cat_label->valuestring ? cat_label->valuestring : "");
            cJSON *actions = cJSON_GetObjectItem(cat, "actions");
            int ac = actions ? cJSON_GetArraySize(actions) : 0;
            data.action_counts[ci] = ac;
            data.action_keys[ci] = malloc(ac * sizeof(char *));
            data.action_descs[ci] = malloc(ac * sizeof(char *));
            int ai = 0;
            cJSON *act;
            cJSON_ArrayForEach(act, actions) {
                cJSON *key_j = cJSON_GetObjectItem(act, "key");
                cJSON *desc_j = cJSON_GetObjectItem(act, "description");
                data.action_keys[ci][ai] = strdup(key_j && key_j->valuestring ? key_j->valuestring : "");
                data.action_descs[ci][ai] = strdup(desc_j && desc_j->valuestring ? desc_j->valuestring : "");
                ai++;
            }
            ci++;
        }
    }
    widget_base_init(w, &data, sizeof(AnvilData), anvil_render, anvil_handle, anvil_destroy);
    return w;
}