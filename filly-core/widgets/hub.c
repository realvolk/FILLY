#include "hub.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct HubItem_s {
    char *id;
    char *label;
    char *value;
    char *widget;
    char **choices;
    int choice_count;
    char *placeholder;
    char *message;
    char *display;
    bool disk_picker;
    struct {
        char **keys;
        char **vals;
        int count;
    } visible_if;
} HubItem;

typedef struct HubCategory_s {
    char *id;
    char *label;
    char *summary_template;
    HubItem *items;
    int item_count;
} HubCategory;

typedef enum {
    HUB_BROWSING,
    HUB_EDITING,
    HUB_CONFIRM_PROCEED,
    HUB_CONFIRM_QUIT
} HubMode;

typedef struct {
    char *title;
    HubCategory *categories;
    int cat_count;
    char **actions;
    int action_count;
    int cat_idx;
    int item_idx;
    char **keys;
    char **vals;
    int val_count;
    HubMode mode;
    int edit_idx;
    Widget *sub_widget;
    bool dirty;
} HubData;

static char *hub_get(HubData *d, const char *key) {
    for (int i = 0; i < d->val_count; i++)
        if (strcmp(d->keys[i], key) == 0) return d->vals[i];
    return NULL;
}

static void hub_set(HubData *d, const char *key, const char *value) {
    for (int i = 0; i < d->val_count; i++) {
        if (strcmp(d->keys[i], key) == 0) { free(d->vals[i]); d->vals[i] = strdup(value); return; }
    }
    d->val_count++;
    d->keys = realloc(d->keys, d->val_count * sizeof(char *));
    d->vals = realloc(d->vals, d->val_count * sizeof(char *));
    d->keys[d->val_count - 1] = strdup(key);
    d->vals[d->val_count - 1] = strdup(value);
}

static bool hub_visible(HubData *d, HubItem *item) {
    if (item->visible_if.count == 0) return true;
    for (int i = 0; i < item->visible_if.count; i++) {
        char *v = hub_get(d, item->visible_if.keys[i]);
        if (!v || strcmp(v, item->visible_if.vals[i]) != 0) return false;
    }
    return true;
}

static void hub_render(Widget *self, Rect area, RenderTree *out) {
    HubData *d = (HubData *)(self + 1);
    memset(out, 0, sizeof(*out));

    if (d->mode == HUB_EDITING && d->sub_widget) {
        d->sub_widget->vtable.render(d->sub_widget, area, out);
        return;
    }

    int box_w = (int)(area.w * 0.95f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.95f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    if (d->mode == HUB_CONFIRM_PROCEED || d->mode == HUB_CONFIRM_QUIT) {
        RenderTree *children = calloc(3, sizeof(RenderTree));
        children[0].type = RNODE_TEXT;
        children[0].rect = rect_new(1, 0, box_w - 2, 1);
        children[0].text.content = strdup(d->title);
        children[0].text.align = ALIGN_CENTER;
        children[0].text.style = textstyle_selected();

        children[1].type = RNODE_TEXT;
        children[1].rect = rect_new(1, 2, box_w - 2, 1);
        children[1].text.content = strdup(d->mode == HUB_CONFIRM_PROCEED ? "Proceed with installation?" : "Quit without saving?");
        children[1].text.align = ALIGN_CENTER;
        children[1].text.style = textstyle_normal();

        children[2].type = RNODE_TEXT;
        children[2].rect = rect_new(1, 4, box_w - 2, 1);
        children[2].text.content = strdup("[Y]es  [N]o");
        children[2].text.align = ALIGN_CENTER;
        children[2].text.style = textstyle_accent();

        out->type = RNODE_CONTAINER;
        out->rect = rect_new(box_x, box_y, box_w, 7);
        out->container.border = BORDER_SINGLE;
        out->container.padding = edgeinsets_zero();
        out->container.children = children;
        out->container.child_count = 3;
        return;
    }

    RenderTree *children = calloc(4, sizeof(RenderTree));
    int idx = 0;

    children[idx].type = RNODE_TEXT;
    children[idx].rect = rect_new(1, 0, box_w - 2, 1);
    children[idx].text.content = strdup(d->title);
    children[idx].text.align = ALIGN_CENTER;
    children[idx].text.style = textstyle_selected();
    idx++;

    int left_w = box_w * 30 / 100;
    int right_x = left_w + 2;
    int right_w = box_w - right_x - 1;

    children[idx].type = RNODE_LIST;
    children[idx].rect = rect_new(1, 1, left_w, box_h - 3);
    children[idx].list.item_count = d->cat_count;
    children[idx].list.items = malloc(d->cat_count * sizeof(ListItem));
    for (int i = 0; i < d->cat_count; i++) {
        char label[256];
        snprintf(label, sizeof(label), "%s %s", i == d->cat_idx ? ">" : " ", d->categories[i].label);
        children[idx].list.items[i] = listitem_new(label);
    }
    children[idx].list.selected = d->cat_idx;
    children[idx].list.highlight = textstyle_selected();
    idx++;

    HubCategory *cat = &d->categories[d->cat_idx];
    int vis_count = 0;
    for (int i = 0; i < cat->item_count; i++) if (hub_visible(d, &cat->items[i])) vis_count++;

    children[idx].type = RNODE_LIST;
    children[idx].rect = rect_new(right_x, 1, right_w, box_h - 3);
    children[idx].list.item_count = vis_count;
    children[idx].list.items = malloc(vis_count * sizeof(ListItem));
    int vi = 0;
    for (int i = 0; i < cat->item_count; i++) {
        if (!hub_visible(d, &cat->items[i])) continue;
        HubItem *item = &cat->items[i];
        char *val = hub_get(d, item->id);
        char *disp = val ? val : item->value;
        if (item->display && strcmp(item->display, "set/not set") == 0)
            disp = (val && strlen(val) > 0) ? "set" : "not set";
        if (!disp || strlen(disp) == 0) disp = "(none)";
        char label[512];
        snprintf(label, sizeof(label), "%s %s: %s", vi == d->item_idx ? ">" : " ", item->label, disp);
        children[idx].list.items[vi] = listitem_new(label);
        vi++;
    }
    children[idx].list.selected = d->item_idx;
    children[idx].list.highlight = textstyle_selected();
    idx++;

    char footer[512] = {0};
    for (int i = 0; i < d->action_count; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "F%d:%s  ", i + 1, d->actions[i]);
        strcat(footer, buf);
    }
    strcat(footer, "Up/Down:items  Left/Right:categories  Enter:edit  Esc:quit");
    children[idx].type = RNODE_TEXT;
    children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[idx].text.content = strdup(footer);
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

static EventResult hub_handle_event(Widget *self, Event *ev, Backend *backend) {
    HubData *d = (HubData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();

    if (d->mode == HUB_CONFIRM_QUIT) {
        if (ev->code == KEY_CHAR && (ev->ch == 'y' || ev->ch == 'Y'))
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        d->mode = HUB_BROWSING; d->dirty = true; return event_result_handled();
    }

    if (d->mode == HUB_CONFIRM_PROCEED) {
        if (ev->code == KEY_CHAR && (ev->ch == 'y' || ev->ch == 'Y')) {
            cJSON *result = cJSON_CreateObject();
            for (int i = 0; i < d->val_count; i++)
                cJSON_AddStringToObject(result, d->keys[i], d->vals[i]);
            return event_result_response((WidgetResponse){ .result = result, .cancelled = false, .error = NULL });
        }
        d->mode = HUB_BROWSING; d->dirty = true; return event_result_handled();
    }

    if (d->mode == HUB_EDITING && d->sub_widget) {
        EventResult r = d->sub_widget->vtable.handle_event(d->sub_widget, ev, backend);
        if (r.type == EVENT_RESULT_RESPONSE) {
            HubCategory *cat = &d->categories[d->cat_idx];
            HubItem *item = NULL;
            int vi = 0;
            for (int i = 0; i < cat->item_count; i++) {
                if (!hub_visible(d, &cat->items[i])) continue;
                if (vi == d->edit_idx) { item = &cat->items[i]; break; }
                vi++;
            }
            if (item && !r.response.cancelled && r.response.result) {
                if (r.response.result->valuestring)
                    hub_set(d, item->id, r.response.result->valuestring);
                else if (r.response.result->type == cJSON_True)
                    hub_set(d, item->id, "yes");
                else if (r.response.result->type == cJSON_False)
                    hub_set(d, item->id, "no");
                else if (r.response.result->type == cJSON_Array) {
                    char *joined = cJSON_PrintUnformatted(r.response.result);
                    hub_set(d, item->id, joined);
                    free(joined);
                }
            }
            widget_destroy(d->sub_widget);
            d->sub_widget = NULL;
            d->mode = HUB_BROWSING;
            d->dirty = true;
        }
        return event_result_handled();
    }

    switch (ev->code) {
        case KEY_ESC: d->mode = HUB_CONFIRM_QUIT; d->dirty = true; return event_result_handled();
        case KEY_UP: if (d->item_idx > 0) d->item_idx--; d->dirty = true; return event_result_handled();
        case KEY_DOWN: {
            HubCategory *c = &d->categories[d->cat_idx];
            int vc = 0;
            for (int i = 0; i < c->item_count; i++) if (hub_visible(d, &c->items[i])) vc++;
            if (d->item_idx + 1 < vc) d->item_idx++;
            d->dirty = true;
            return event_result_handled();
        }
        case KEY_LEFT: if (d->cat_idx > 0) d->cat_idx--; else d->cat_idx = d->cat_count - 1; d->item_idx = 0; d->dirty = true; return event_result_handled();
        case KEY_RIGHT: case KEY_TAB: if (d->cat_idx + 1 < d->cat_count) d->cat_idx++; else d->cat_idx = 0; d->item_idx = 0; d->dirty = true; return event_result_handled();
        case KEY_ENTER: {
            HubCategory *cat = &d->categories[d->cat_idx];
            HubItem *item = NULL;
            int vi = 0;
            for (int i = 0; i < cat->item_count; i++) {
                if (!hub_visible(d, &cat->items[i])) continue;
                if (vi == d->item_idx) { item = &cat->items[i]; break; }
                vi++;
            }
            if (!item) return event_result_handled();

            char *current = hub_get(d, item->id);
            if (!current) current = item->value;

            WidgetRequest req = { .widget = item->widget, .params = cJSON_CreateObject() };
            cJSON_AddStringToObject(req.params, "title", item->label);
            if (item->message && strlen(item->message)) cJSON_AddStringToObject(req.params, "message", item->message);
            if (current && strlen(current)) cJSON_AddStringToObject(req.params, "default", current);
            if (item->placeholder && strlen(item->placeholder)) cJSON_AddStringToObject(req.params, "placeholder", item->placeholder);
            if (item->choice_count > 0) {
                cJSON *ch = cJSON_CreateArray();
                for (int i = 0; i < item->choice_count; i++) cJSON_AddItemToArray(ch, cJSON_CreateString(item->choices[i]));
                cJSON_AddItemToObject(req.params, "choices", ch);
            }
            if (item->disk_picker) {
                cJSON_AddBoolToObject(req.params, "disk_picker", 1);
                FILE *fp = popen("lsblk -dpno NAME,SIZE,MODEL -e 7 2>/dev/null", "r");
                if (fp) {
                    cJSON *disks = cJSON_CreateArray();
                    char buf[256];
                    while (fgets(buf, sizeof(buf), fp)) {
                        buf[strcspn(buf, "\n")] = 0;
                        char label[512];
                        snprintf(label, sizeof(label), "%s", buf);
                        cJSON_AddItemToArray(disks, cJSON_CreateString(label));
                    }
                    pclose(fp);
                    cJSON_ReplaceItemInObject(req.params, "choices", disks);
                }
            }

            d->sub_widget = widget_registry_create(&req);
            cJSON_Delete(req.params);
            if (d->sub_widget) {
                d->edit_idx = d->item_idx;
                d->mode = HUB_EDITING;
                d->dirty = true;
            }
            return event_result_handled();
        }
        default: {
            if (ev->code >= KEY_F1 && ev->code <= KEY_F12) {
                int f = ev->code - KEY_F1;
                if (f < d->action_count) {
                    if (strcmp(d->actions[f], "Proceed") == 0) {
                        d->mode = HUB_CONFIRM_PROCEED;
                        d->dirty = true;
                        return event_result_handled();
                    }
                    return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->actions[f]), .cancelled = false, .error = NULL });
                }
            }
            return event_result_unhandled();
        }
    }
}

static bool hub_is_dirty(Widget *self) {
    HubData *d = (HubData *)(self + 1);
    if (d->dirty) return true;
    if (d->mode == HUB_EDITING && d->sub_widget)
        return d->sub_widget->vtable.is_dirty(d->sub_widget);
    return false;
}

static void hub_clear_dirty(Widget *self) {
    HubData *d = (HubData *)(self + 1);
    d->dirty = false;
    if (d->mode == HUB_EDITING && d->sub_widget)
        d->sub_widget->vtable.clear_dirty(d->sub_widget);
}

static void hub_destroy(Widget *self) {
    HubData *d = (HubData *)(self + 1);
    free(d->title);
    for (int i = 0; i < d->cat_count; i++) {
        free(d->categories[i].id); free(d->categories[i].label); free(d->categories[i].summary_template);
        for (int j = 0; j < d->categories[i].item_count; j++) {
            HubItem *item = &d->categories[i].items[j];
            free(item->id); free(item->label); free(item->value); free(item->widget);
            for (int k = 0; k < item->choice_count; k++) free(item->choices[k]);
            free(item->choices); free(item->placeholder); free(item->message); free(item->display);
            for (int k = 0; k < item->visible_if.count; k++) { free(item->visible_if.keys[k]); free(item->visible_if.vals[k]); }
            free(item->visible_if.keys); free(item->visible_if.vals);
        }
        free(d->categories[i].items);
    }
    free(d->categories);
    for (int i = 0; i < d->action_count; i++) free(d->actions[i]);
    free(d->actions);
    for (int i = 0; i < d->val_count; i++) { free(d->keys[i]); free(d->vals[i]); }
    free(d->keys); free(d->vals);
    if (d->sub_widget) widget_destroy(d->sub_widget);
}

Widget *hub_widget_new(const char *title, cJSON *categories_json, cJSON *actions_json) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(HubData));
    w->vtable.render = hub_render;
    w->vtable.handle_event = hub_handle_event;
    w->vtable.is_dirty = hub_is_dirty;
    w->vtable.clear_dirty = hub_clear_dirty;
    w->vtable.destroy = hub_destroy;
    HubData *d = (HubData *)(w + 1);
    d->title = strdup(title);
    d->cat_idx = 0; d->item_idx = 0;
    d->keys = NULL; d->vals = NULL; d->val_count = 0;
    d->mode = HUB_BROWSING;
    d->sub_widget = NULL;
    d->dirty = true;

    d->cat_count = 0; d->categories = NULL;
    if (categories_json && categories_json->type == cJSON_Array) {
        d->cat_count = cJSON_GetArraySize(categories_json);
        d->categories = calloc(d->cat_count, sizeof(HubCategory));
        int ci = 0;
        cJSON *cat_val;
        cJSON_ArrayForEach(cat_val, categories_json) {
            cJSON *cid = cJSON_GetObjectItem(cat_val, "id");
            cJSON *clabel = cJSON_GetObjectItem(cat_val, "label");
            cJSON *ctmpl = cJSON_GetObjectItem(cat_val, "summary_template");
            d->categories[ci].id = cid && cid->valuestring ? strdup(cid->valuestring) : strdup("");
            d->categories[ci].label = clabel && clabel->valuestring ? strdup(clabel->valuestring) : strdup("");
            d->categories[ci].summary_template = ctmpl && ctmpl->valuestring ? strdup(ctmpl->valuestring) : strdup("");

            cJSON *items_arr = cJSON_GetObjectItem(cat_val, "items");
            d->categories[ci].item_count = items_arr ? cJSON_GetArraySize(items_arr) : 0;
            d->categories[ci].items = calloc(d->categories[ci].item_count, sizeof(HubItem));
            int ii = 0;
            cJSON *item_val;
            cJSON_ArrayForEach(item_val, items_arr) {
                HubItem *item = &d->categories[ci].items[ii];
                cJSON *iid = cJSON_GetObjectItem(item_val, "id");
                item->id = iid && iid->valuestring ? strdup(iid->valuestring) : strdup("");
                cJSON *il = cJSON_GetObjectItem(item_val, "label");
                item->label = il && il->valuestring ? strdup(il->valuestring) : strdup("");
                cJSON *iv = cJSON_GetObjectItem(item_val, "value");
                item->value = iv && iv->valuestring ? strdup(iv->valuestring) : strdup("");
                cJSON *iw = cJSON_GetObjectItem(item_val, "widget");
                item->widget = iw && iw->valuestring ? strdup(iw->valuestring) : strdup("menu");
                cJSON *ich = cJSON_GetObjectItem(item_val, "choices");
                item->choice_count = ich ? cJSON_GetArraySize(ich) : 0;
                item->choices = item->choice_count > 0 ? malloc(item->choice_count * sizeof(char *)) : NULL;
                for (int j = 0; j < item->choice_count; j++)
                    item->choices[j] = strdup(cJSON_GetArrayItem(ich, j)->valuestring);
                cJSON *ip = cJSON_GetObjectItem(item_val, "placeholder");
                item->placeholder = ip && ip->valuestring ? strdup(ip->valuestring) : strdup("");
                cJSON *im = cJSON_GetObjectItem(item_val, "message");
                item->message = im && im->valuestring ? strdup(im->valuestring) : strdup("");
                cJSON *idisp = cJSON_GetObjectItem(item_val, "display");
                item->display = idisp && idisp->valuestring ? strdup(idisp->valuestring) : NULL;
                cJSON *dp = cJSON_GetObjectItem(item_val, "disk_picker");
                item->disk_picker = dp && dp->valueint;
                cJSON *vi = cJSON_GetObjectItem(item_val, "visible_if");
                item->visible_if.keys = NULL; item->visible_if.vals = NULL; item->visible_if.count = 0;
                if (vi && vi->type == cJSON_Object) {
                    item->visible_if.count = cJSON_GetArraySize(vi);
                    item->visible_if.keys = malloc(item->visible_if.count * sizeof(char *));
                    item->visible_if.vals = malloc(item->visible_if.count * sizeof(char *));
                    cJSON *child = vi->child;
                    int k = 0;
                    while (child) {
                        item->visible_if.keys[k] = strdup(child->string);
                        item->visible_if.vals[k] = child->valuestring ? strdup(child->valuestring) : strdup("");
                        k++; child = child->next;
                    }
                }
                hub_set(d, item->id, item->value);
                ii++;
            }
            ci++;
        }
    }

    d->action_count = 0; d->actions = NULL;
    if (actions_json && actions_json->type == cJSON_Array) {
        d->action_count = cJSON_GetArraySize(actions_json);
        d->actions = malloc(d->action_count * sizeof(char *));
        for (int i = 0; i < d->action_count; i++)
            d->actions[i] = strdup(cJSON_GetArrayItem(actions_json, i)->valuestring);
    }

    return w;
}