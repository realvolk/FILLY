#include "core/widget.h"
#include "core/render.h"
#include "core/widget_base.h"
#include "core/session.h"
#include "protocol/protocol.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

typedef struct ForgeItem_s {
    char *id, *label, *value, *widget, **choices, *placeholder, *message, *display;
    int choice_count;
    struct { char **keys, **vals; int count; } visible_if;
} ForgeItem;

typedef struct ForgeCategory_s {
    char *id, *label, *summary_template;
    ForgeItem *items;
    int item_count;
} ForgeCategory;

typedef enum {
    FHUB_BROWSING, FHUB_EDITING_MENU, FHUB_EDITING_INPUT,
    FHUB_EDITING_PASSWORD, FHUB_EDITING_YESNO, FHUB_EDITING_FILTER,
    FHUB_EDITING_MULTISELECT, FHUB_EDITING_SUB_WIDGET,
    FHUB_VIEW_LOG, FHUB_VIEW_QUEUE, FHUB_CONFIRM_QUIT, FHUB_CONFIRM_PROCEED
} ForgeHubMode;

typedef struct {
    WidgetBase base;
    char *title;
    ForgeCategory *categories; int cat_count;
    char **actions; int action_count;
    int cat_idx, item_idx;
    char **keys, **vals; int val_count;
    ForgeHubMode mode;
    int edit_selected, edit_cursor;
    char *edit_text, *edit_pass1, *edit_pass2;
    bool edit_pass_field, edit_yes;
    char *edit_query;
    int *edit_filtered, edit_filtered_count;
    bool *edit_selected_set;
    int edit_min_sel, edit_max_sel;
    Widget *sub_widget;
    WidgetRequest sub_req;
    char *build_stage;
    int build_current, build_total;
    char **build_queue; int build_queue_count;
    int build_failed;
} ForgeHubData;

extern Arena *g_session_arena;

static char *fh_get(ForgeHubData *d, const char *key) {
    for (int i = 0; i < d->val_count; i++)
        if (strcmp(d->keys[i], key) == 0) return d->vals[i];
    return NULL;
}

static void fh_set(ForgeHubData *d, const char *key, const char *value) {
    for (int i = 0; i < d->val_count; i++) {
        if (strcmp(d->keys[i], key) == 0) { free(d->vals[i]); d->vals[i] = strdup(value); return; }
    }
    d->val_count++;
    d->keys = realloc(d->keys, d->val_count * sizeof(char *));
    d->vals = realloc(d->vals, d->val_count * sizeof(char *));
    d->keys[d->val_count-1] = strdup(key);
    d->vals[d->val_count-1] = strdup(value);
}

static bool fh_visible(ForgeHubData *d, ForgeItem *item) {
    if (item->visible_if.count == 0) return true;
    for (int i = 0; i < item->visible_if.count; i++) {
        char *v = fh_get(d, item->visible_if.keys[i]);
        if (!v) return false;
        char *cond = strdup(item->visible_if.vals[i]);
        char *tok = strtok(cond, ",");
        bool matched = false;
        while (tok) { while (*tok == ' ') tok++; if (strcmp(v, tok) == 0) { matched = true; break; } tok = strtok(NULL, ","); }
        free(cond);
        if (!matched) return false;
    }
    return true;
}

static ForgeItem *fh_get_item(ForgeHubData *d, int *out_vi) {
    ForgeCategory *cat = &d->categories[d->cat_idx];
    int vi = 0;
    for (int i = 0; i < cat->item_count; i++) {
        if (!fh_visible(d, &cat->items[i])) continue;
        if (vi == d->item_idx) { if (out_vi) *out_vi = vi; return &cat->items[i]; }
        vi++;
    }
    return NULL;
}

static int fh_visible_count(ForgeHubData *d) {
    ForgeCategory *cat = &d->categories[d->cat_idx];
    int vc = 0;
    for (int i = 0; i < cat->item_count; i++)
        if (fh_visible(d, &cat->items[i])) vc++;
    return vc;
}

static void fh_update_filter(ForgeHubData *d, char **choices, int count) {
    free(d->edit_filtered);
    if (!d->edit_query || strlen(d->edit_query) == 0) {
        d->edit_filtered = malloc(count * sizeof(int));
        d->edit_filtered_count = count;
        for (int i = 0; i < count; i++) d->edit_filtered[i] = i;
    } else {
        d->edit_filtered = malloc(count * sizeof(int));
        d->edit_filtered_count = 0;
        char *ql = strdup(d->edit_query);
        for (int i = 0; ql[i]; i++) ql[i] = tolower(ql[i]);
        for (int i = 0; i < count; i++) {
            char *cl = strdup(choices[i]);
            for (int j = 0; cl[j]; j++) cl[j] = tolower(cl[j]);
            if (strstr(cl, ql)) d->edit_filtered[d->edit_filtered_count++] = i;
            free(cl);
        }
        free(ql);
    }
    if (d->edit_selected >= d->edit_filtered_count) d->edit_selected = d->edit_filtered_count > 0 ? d->edit_filtered_count - 1 : 0;
}

static void fh_enter_edit(ForgeHubData *d, ForgeItem *item) {
    char *current = fh_get(d, item->id);
    if (!current) current = item->value;
    if (strcmp(item->widget, "menu") == 0 || strcmp(item->widget, "filter") == 0) {
        d->edit_selected = 0;
        for (int i = 0; i < item->choice_count; i++)
            if (strcmp(item->choices[i], current) == 0) { d->edit_selected = i; break; }
        if (strcmp(item->widget, "filter") == 0) {
            free(d->edit_query); d->edit_query = strdup("");
            fh_update_filter(d, item->choices, item->choice_count);
            d->mode = FHUB_EDITING_FILTER;
        } else d->mode = FHUB_EDITING_MENU;
    } else if (strcmp(item->widget, "input") == 0) {
        free(d->edit_text); d->edit_text = strdup(current);
        d->edit_cursor = strlen(d->edit_text); d->mode = FHUB_EDITING_INPUT;
    } else if (strcmp(item->widget, "password") == 0 || strcmp(item->widget, "password_confirm") == 0) {
        free(d->edit_pass1); free(d->edit_pass2);
        d->edit_pass1 = strdup(""); d->edit_pass2 = strdup("");
        d->edit_pass_field = false; d->mode = FHUB_EDITING_PASSWORD;
    } else if (strcmp(item->widget, "yesno") == 0) {
        d->edit_yes = (strcmp(current, "yes") == 0); d->mode = FHUB_EDITING_YESNO;
    } else if (strcmp(item->widget, "multiselect") == 0) {
        free(d->edit_query); d->edit_query = strdup("");
        free(d->edit_selected_set);
        d->edit_selected_set = calloc(item->choice_count, sizeof(bool));
        d->edit_min_sel = 0; d->edit_max_sel = item->choice_count;
        fh_update_filter(d, item->choices, item->choice_count);
        d->edit_selected = 0; d->mode = FHUB_EDITING_MULTISELECT;
    } else {
        memset(&d->sub_req, 0, sizeof(d->sub_req));
        d->sub_req.widget = item->widget;
        d->sub_req.params = cJSON_CreateObject();
        cJSON_AddStringToObject(d->sub_req.params, "title", item->label);
        if (item->message && strlen(item->message))
            cJSON_AddStringToObject(d->sub_req.params, "message", item->message);
        if (current && strlen(current))
            cJSON_AddStringToObject(d->sub_req.params, "default", current);
        if (item->placeholder && strlen(item->placeholder))
            cJSON_AddStringToObject(d->sub_req.params, "placeholder", item->placeholder);
        if (item->choice_count > 0) {
            cJSON *ch = cJSON_CreateArray();
            for (int i = 0; i < item->choice_count; i++)
                cJSON_AddItemToArray(ch, cJSON_CreateString(item->choices[i]));
            cJSON_AddItemToObject(d->sub_req.params, "choices", ch);
        }
        d->sub_widget = widget_registry_create(&d->sub_req);
        d->mode = FHUB_EDITING_SUB_WIDGET;
    }
    d->base.dirty = true;
}

static void fh_render_overlay(ForgeHubData *d, Rect area, RenderTree *out) {
    if (d->mode == FHUB_EDITING_SUB_WIDGET && d->sub_widget) {
        d->sub_widget->vtable.render(d->sub_widget, out); return;
    }
    if (d->mode == FHUB_VIEW_LOG) {
        int ow = (int)(area.w * 0.8f), oh = (int)(area.h * 0.8f);
        if (ow > area.w - 2) ow = area.w - 2;
        if (oh > area.h - 2) oh = area.h - 2;
        int ox = (area.w - ow) / 2, oy = (area.h - oh) / 2;
        RenderTree *children = arena_alloc(g_session_arena, 2 * sizeof(RenderTree));
        children[0].type = RNODE_TEXT;
        children[0].rect = rect_new(1, 0, ow - 2, 1);
        children[0].u.text.content = "Build Log (last 50 lines)";
        children[0].style_class = "text"; children[0].state = "title";
        children[1].type = RNODE_TEXT;
        children[1].rect = rect_new(1, 1, ow - 2, oh - 3);
        children[1].u.text.content = "Log viewer — press Esc to return";
        children[1].style_class = "text"; children[1].state = "muted";
        out->type = RNODE_CONTAINER;
        out->rect = rect_new(ox, oy, ow, oh);
        out->u.container.border = BORDER_SINGLE;
        out->u.container.padding = edgeinsets_zero();
        out->u.container.children = children;
        out->u.container.child_count = 2;
        return;
    }
    if (d->mode == FHUB_VIEW_QUEUE) {
        int ow = (int)(area.w * 0.8f), oh = (int)(area.h * 0.8f);
        if (ow > area.w - 2) ow = area.w - 2;
        if (oh > area.h - 2) oh = area.h - 2;
        int ox = (area.w - ow) / 2, oy = (area.h - oh) / 2;
        int lines = d->build_queue_count + 2;
        RenderTree *children = arena_alloc(g_session_arena, lines * sizeof(RenderTree));
        children[0].type = RNODE_TEXT;
        children[0].rect = rect_new(1, 0, ow - 2, 1);
        char stage_buf[128];
        snprintf(stage_buf, sizeof(stage_buf), "Stage: %s  [%d/%d]  Failed: %d",
            d->build_stage ? d->build_stage : "idle", d->build_current, d->build_total, d->build_failed);
        children[0].u.text.content = arena_strdup(g_session_arena, stage_buf);
        children[0].style_class = "text"; children[0].state = "title";
        for (int i = 0; i < d->build_queue_count; i++) {
            children[1 + i].type = RNODE_TEXT;
            children[1 + i].rect = rect_new(1, 1 + i, ow - 2, 1);
            children[1 + i].u.text.content = arena_strdup(g_session_arena, d->build_queue[i]);
            children[1 + i].style_class = "text";
        }
        children[lines - 1].type = RNODE_TEXT;
        children[lines - 1].rect = rect_new(1, oh - 2, ow - 2, 1);
        children[lines - 1].u.text.content = "Esc:return  F5:retry  F4:skip";
        children[lines - 1].style_class = "text"; children[lines - 1].state = "muted";
        out->type = RNODE_CONTAINER;
        out->rect = rect_new(ox, oy, ow, oh);
        out->u.container.border = BORDER_SINGLE;
        out->u.container.padding = edgeinsets_zero();
        out->u.container.children = children;
        out->u.container.child_count = lines;
        return;
    }
    int ow = (int)(area.w * 0.55f), oh = (int)(area.h * 0.60f);
    if (ow > area.w - 4) ow = area.w - 4;
    if (oh > area.h - 4) oh = area.h - 4;
    int ox = (area.w - ow) / 2, oy = (area.h - oh) / 2;
    RenderTree *children = NULL; int child_count = 0;
    ForgeItem *item = fh_get_item(d, NULL);
    const char *msg = item && item->message ? item->message : "";
    int msg_lines = 0;
    for (const char *p = msg; *p; p++) if (*p == '\n') msg_lines++;
    int msg_h = strlen(msg) > 0 ? msg_lines + 1 : 0;

    switch (d->mode) {
    case FHUB_EDITING_MENU: {
        child_count = 3 + (msg_h > 0 ? 1 : 0);
        children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree));
        int ci = 0;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, 0, ow - 2, 1);
        children[ci].u.text.content = arena_strdup(g_session_arena, item ? item->label : "");
        children[ci].style_class = "text"; children[ci].state = "title"; ci++;
        int list_y = 1;
        if (msg_h > 0) {
            children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, 1, ow - 2, msg_h);
            children[ci].u.text.content = arena_strdup(g_session_arena, msg);
            children[ci].style_class = "text"; ci++; list_y = 1 + msg_h;
        }
        children[ci].type = RNODE_LIST; children[ci].rect = rect_new(1, list_y, ow - 2, oh - list_y - 2);
        children[ci].u.list.item_count = item ? item->choice_count : 0;
        children[ci].u.list.selected = d->edit_selected;
        children[ci].u.list.items = arena_alloc(g_session_arena, (item ? item->choice_count : 0) * sizeof(ListItem));
        if (item) for (int i = 0; i < item->choice_count; i++)
            children[ci].u.list.items[i].label = arena_strdup(g_session_arena, item->choices[i]);
        children[ci].style_class = "list"; ci++;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, oh - 2, ow - 2, 1);
        children[ci].u.text.content = "Up/Down:move  Enter:select  Esc:cancel";
        children[ci].style_class = "text"; children[ci].state = "muted";
        break;
    }
    case FHUB_EDITING_INPUT: {
        child_count = 3 + (msg_h > 0 ? 1 : 0);
        children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree));
        int ci = 0;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, 0, ow - 2, 1);
        children[ci].u.text.content = arena_strdup(g_session_arena, item ? item->label : "");
        children[ci].style_class = "text"; children[ci].state = "title"; ci++;
        int input_y = 1;
        if (msg_h > 0) {
            children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, 1, ow - 2, msg_h);
            children[ci].u.text.content = arena_strdup(g_session_arena, msg);
            children[ci].style_class = "text"; ci++; input_y = 1 + msg_h;
        }
        children[ci].type = RNODE_INPUT; children[ci].rect = rect_new(1, input_y, ow - 2, 1);
        children[ci].u.input.text = arena_strdup(g_session_arena, d->edit_text ? d->edit_text : "");
        children[ci].u.input.cursor = d->edit_cursor;
        children[ci].u.input.placeholder = arena_strdup(g_session_arena, item && item->placeholder ? item->placeholder : "");
        children[ci].style_class = "input"; ci++;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, oh - 2, ow - 2, 1);
        children[ci].u.text.content = "Enter:confirm  Esc:cancel";
        children[ci].style_class = "text"; children[ci].state = "muted";
        break;
    }
    case FHUB_EDITING_PASSWORD: {
        child_count = 4 + (msg_h > 0 ? 1 : 0);
        children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree));
        int ci = 0;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, 0, ow - 2, 1);
        children[ci].u.text.content = arena_strdup(g_session_arena, item ? item->label : "");
        children[ci].style_class = "text"; children[ci].state = "title"; ci++;
        int y = 1;
        if (msg_h > 0) {
            children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, y, ow - 2, msg_h);
            children[ci].u.text.content = arena_strdup(g_session_arena, msg);
            children[ci].style_class = "text"; ci++; y += msg_h;
        }
        char mask1[128] = {0};
        for (int j = 0; j < (int)strlen(d->edit_pass1) && j < 127; j++) mask1[j] = '*';
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, y, ow - 2, 1);
        char buf[256]; snprintf(buf, sizeof(buf), "Password: %s", mask1);
        children[ci].u.text.content = arena_strdup(g_session_arena, buf);
        children[ci].style_class = "text"; ci++; y++;
        char mask2[128] = {0};
        for (int j = 0; j < (int)strlen(d->edit_pass2) && j < 127; j++) mask2[j] = '*';
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, y, ow - 2, 1);
        snprintf(buf, sizeof(buf), "Confirm:  %s", mask2);
        children[ci].u.text.content = arena_strdup(g_session_arena, buf);
        children[ci].style_class = "text"; ci++; y++;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, oh - 2, ow - 2, 1);
        children[ci].u.text.content = "Tab:next  Enter:submit  Esc:cancel";
        children[ci].style_class = "text"; children[ci].state = "muted";
        break;
    }
    case FHUB_EDITING_YESNO: {
        child_count = 3 + (msg_h > 0 ? 1 : 0);
        children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree));
        int ci = 0;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, 0, ow - 2, 1);
        children[ci].u.text.content = arena_strdup(g_session_arena, item ? item->label : "");
        children[ci].style_class = "text"; children[ci].state = "title"; ci++;
        int y = 1;
        if (msg_h > 0) {
            children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, y, ow - 2, msg_h);
            children[ci].u.text.content = arena_strdup(g_session_arena, msg);
            children[ci].style_class = "text"; ci++; y += msg_h;
        }
        char yesno_text[64];
        snprintf(yesno_text, sizeof(yesno_text), "[ %s ]  [ %s ]", d->edit_yes ? "Yes" : "yes", d->edit_yes ? "no" : "No");
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, y + 1, ow - 2, 1);
        children[ci].u.text.content = arena_strdup(g_session_arena, yesno_text);
        children[ci].style_class = "text"; ci++;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, oh - 2, ow - 2, 1);
        children[ci].u.text.content = "Left/Right:choose  Enter:confirm  y/n:quick  Esc:cancel";
        children[ci].style_class = "text"; children[ci].state = "muted";
        break;
    }
    case FHUB_EDITING_FILTER: {
        child_count = 4 + (msg_h > 0 ? 1 : 0);
        children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree));
        int ci = 0;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, 0, ow - 2, 1);
        children[ci].u.text.content = arena_strdup(g_session_arena, item ? item->label : "");
        children[ci].style_class = "text"; children[ci].state = "title"; ci++;
        int y = 1;
        if (msg_h > 0) {
            children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, y, ow - 2, msg_h);
            children[ci].u.text.content = arena_strdup(g_session_arena, msg);
            children[ci].style_class = "text"; ci++; y += msg_h;
        }
        children[ci].type = RNODE_INPUT; children[ci].rect = rect_new(1, y, ow - 2, 1);
        char qd[256]; snprintf(qd, sizeof(qd), "> %s", d->edit_query ? d->edit_query : "");
        children[ci].u.input.text = arena_strdup(g_session_arena, qd);
        children[ci].u.input.cursor = strlen(qd);
        children[ci].u.input.placeholder = "Type to filter..."; children[ci].style_class = "input"; ci++; y++;
        children[ci].type = RNODE_LIST; children[ci].rect = rect_new(1, y, ow - 2, oh - y - 2);
        children[ci].u.list.item_count = d->edit_filtered_count;
        children[ci].u.list.selected = d->edit_selected;
        children[ci].u.list.items = arena_alloc(g_session_arena, d->edit_filtered_count * sizeof(ListItem));
        for (int i = 0; i < d->edit_filtered_count; i++)
            children[ci].u.list.items[i].label = arena_strdup(g_session_arena, item->choices[d->edit_filtered[i]]);
        children[ci].style_class = "list"; ci++;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, oh - 2, ow - 2, 1);
        children[ci].u.text.content = "Type:filter  Up/Down:move  Enter:select  Esc:cancel";
        children[ci].style_class = "text"; children[ci].state = "muted";
        break;
    }
    case FHUB_EDITING_MULTISELECT: {
        child_count = 4 + (msg_h > 0 ? 1 : 0);
        children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree));
        int ci = 0;
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, 0, ow - 2, 1);
        children[ci].u.text.content = arena_strdup(g_session_arena, item ? item->label : "");
        children[ci].style_class = "text"; children[ci].state = "title"; ci++;
        int y = 1;
        if (msg_h > 0) {
            children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, y, ow - 2, msg_h);
            children[ci].u.text.content = arena_strdup(g_session_arena, msg);
            children[ci].style_class = "text"; ci++; y += msg_h;
        }
        children[ci].type = RNODE_INPUT; children[ci].rect = rect_new(1, y, ow - 2, 1);
        char qd2[256]; snprintf(qd2, sizeof(qd2), "> %s", d->edit_query ? d->edit_query : "");
        children[ci].u.input.text = arena_strdup(g_session_arena, qd2);
        children[ci].u.input.cursor = strlen(qd2);
        children[ci].u.input.placeholder = "Type to filter..."; children[ci].style_class = "input"; ci++; y++;
        children[ci].type = RNODE_LIST; children[ci].rect = rect_new(1, y, ow - 2, oh - y - 2);
        children[ci].u.list.item_count = d->edit_filtered_count;
        children[ci].u.list.selected = d->edit_selected;
        children[ci].u.list.items = arena_alloc(g_session_arena, d->edit_filtered_count * sizeof(ListItem));
        for (int i = 0; i < d->edit_filtered_count; i++) {
            int orig = d->edit_filtered[i]; char label[512];
            snprintf(label, sizeof(label), "%s %s", d->edit_selected_set[orig] ? "[x]" : "[ ]", item->choices[orig]);
            children[ci].u.list.items[i].label = arena_strdup(g_session_arena, label);
        }
        children[ci].style_class = "list"; ci++;
        int sel_count = 0;
        for (int i = 0; i < item->choice_count; i++) if (d->edit_selected_set[i]) sel_count++;
        char footer[256];
        snprintf(footer, sizeof(footer), "%d selected  Space:toggle  Enter:confirm  Esc:cancel", sel_count);
        children[ci].type = RNODE_TEXT; children[ci].rect = rect_new(1, oh - 2, ow - 2, 1);
        children[ci].u.text.content = arena_strdup(g_session_arena, footer);
        children[ci].style_class = "text"; children[ci].state = "muted";
        break;
    }
    default: break;
    }
    out->type = RNODE_CONTAINER; out->rect = rect_new(ox, oy, ow, oh);
    out->u.container.border = BORDER_SINGLE; out->u.container.padding = edgeinsets_zero();
    out->u.container.children = children; out->u.container.child_count = child_count;
}

static void fh_render(Widget *self, RenderTree *out) {
    ForgeHubData *d = (ForgeHubData *)(self + 1);
    WidgetBase *base = (WidgetBase *)(self + 1);
    Rect area = base->render_area;
    memset(out, 0, sizeof(*out));
    if (d->mode == FHUB_EDITING_SUB_WIDGET && d->sub_widget) { d->sub_widget->vtable.render(d->sub_widget, out); return; }
    if (d->mode == FHUB_VIEW_LOG || d->mode == FHUB_VIEW_QUEUE) { fh_render_overlay(d, area, out); return; }
    if (d->mode >= FHUB_EDITING_MENU && d->mode <= FHUB_EDITING_MULTISELECT) { fh_render_overlay(d, area, out); return; }
    out->style_class = "container";
    int box_w = (int)(area.w * 0.95f); if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.92f); if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    if (d->mode == FHUB_CONFIRM_PROCEED || d->mode == FHUB_CONFIRM_QUIT) {
        RenderTree *children = arena_alloc(g_session_arena, 3 * sizeof(RenderTree));
        children[0].type = RNODE_TEXT; children[0].rect = rect_new(1, 0, box_w - 2, 1);
        children[0].u.text.content = arena_strdup(g_session_arena, d->title);
        children[0].style_class = "text"; children[0].state = "title";
        children[1].type = RNODE_TEXT; children[1].rect = rect_new(1, 2, box_w - 2, 1);
        children[1].u.text.content = d->mode == FHUB_CONFIRM_PROCEED ? "Proceed with build?" : "Quit without saving?";
        children[1].style_class = "text";
        children[2].type = RNODE_TEXT; children[2].rect = rect_new(1, 4, box_w - 2, 1);
        children[2].u.text.content = "[Y]es  [N]o"; children[2].style_class = "text";
        out->type = RNODE_CONTAINER; out->rect = rect_new(box_x, box_y, box_w, 7);
        out->u.container.border = BORDER_SINGLE; out->u.container.padding = edgeinsets_zero();
        out->u.container.children = children; out->u.container.child_count = 3;
        return;
    }

    int child_count = 4;
    if (d->build_queue_count > 0) child_count++;
    RenderTree *children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree));
    int idx = 0;

    children[idx].type = RNODE_TEXT;
    children[idx].rect = rect_new(1, 0, box_w - 2, 1);
    children[idx].u.text.content = arena_strdup(g_session_arena, d->title);
    children[idx].style_class = "text"; children[idx].state = "title"; idx++;

    if (d->build_queue_count > 0) {
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, 1, box_w - 2, 1);
        char buf[256];
        snprintf(buf, sizeof(buf), "Stage: %s  [%d/%d]  Failed: %d  F2:Log  F3:Queue  F5:Retry  F10:Abort",
            d->build_stage ? d->build_stage : "idle", d->build_current, d->build_total, d->build_failed);
        children[idx].u.text.content = arena_strdup(g_session_arena, buf);
        children[idx].style_class = "text"; children[idx].state = "muted"; idx++;
    }

    int left_w = box_w * 30 / 100, right_x = left_w + 2, right_w = box_w - right_x - 1;
    int list_start_y = (d->build_queue_count > 0) ? 2 : 1;

    children[idx].type = RNODE_LIST;
    children[idx].rect = rect_new(1, list_start_y, left_w, box_h - list_start_y - 2);
    children[idx].u.list.item_count = d->cat_count;
    children[idx].u.list.selected = d->cat_idx;
    children[idx].u.list.items = arena_alloc(g_session_arena, d->cat_count * sizeof(ListItem));
    for (int i = 0; i < d->cat_count; i++) {
        char label[256];
        snprintf(label, sizeof(label), "%s %s", i == d->cat_idx ? ">" : " ", d->categories[i].label);
        children[idx].u.list.items[i].label = arena_strdup(g_session_arena, label);
    }
    children[idx].style_class = "list"; idx++;

    ForgeCategory *cat = NULL;
    int vis = 0;
    if (d->cat_count > 0) { cat = &d->categories[d->cat_idx]; vis = fh_visible_count(d); }
    children[idx].type = RNODE_LIST;
    children[idx].rect = rect_new(right_x, list_start_y, right_w, box_h - list_start_y - 2);
    children[idx].u.list.item_count = vis;
    children[idx].u.list.selected = d->item_idx;
    children[idx].u.list.items = vis > 0 ? arena_alloc(g_session_arena, vis * sizeof(ListItem)) : NULL;
    int vi = 0;
    if (cat) {
        for (int i = 0; i < cat->item_count; i++) {
            if (!fh_visible(d, &cat->items[i])) continue;
            ForgeItem *item = &cat->items[i]; char *val = fh_get(d, item->id);
            char *disp = val ? val : item->value;
            if (item->display && strcmp(item->display, "set/not set") == 0)
                disp = (val && strlen(val) > 0) ? "set" : "not set";
            if (!disp || strlen(disp) == 0) disp = "(none)";
            char label[512];
            snprintf(label, sizeof(label), "%s %s: %s", vi == d->item_idx ? ">" : " ", item->label, disp);
            children[idx].u.list.items[vi].label = arena_strdup(g_session_arena, label); vi++;
        }
    }
    children[idx].style_class = "list"; idx++;

    char footer[512] = {0};
    for (int i = 0; i < d->action_count; i++) {
        char b[32]; snprintf(b, sizeof(b), "F%d:%s  ", i + 1, d->actions[i]); strcat(footer, b);
    }
    strcat(footer, "Up/Down:items  Left/Right:categories  Enter:edit  Esc:quit");
    children[idx].type = RNODE_TEXT;
    children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[idx].u.text.content = arena_strdup(g_session_arena, footer);
    children[idx].style_class = "text"; children[idx].state = "muted"; idx++;

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->u.container.border = BORDER_SINGLE;
    out->u.container.padding = edgeinsets_zero();
    out->u.container.children = children;
    out->u.container.child_count = idx;
}

static EventResult fh_handle_edit_event(ForgeHubData *d, Event *ev) {
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    if (d->mode == FHUB_EDITING_SUB_WIDGET && d->sub_widget) {
        EventResult r = d->sub_widget->vtable.handle_event(d->sub_widget, ev, NULL);
        if (r.type == EVENT_RESULT_RESPONSE) {
            ForgeItem *item = fh_get_item(d, NULL);
            if (item && !r.response.cancelled && r.response.result) {
                if (r.response.result->valuestring) fh_set(d, item->id, r.response.result->valuestring);
                else if (r.response.result->type == cJSON_True) fh_set(d, item->id, "yes");
                else if (r.response.result->type == cJSON_False) fh_set(d, item->id, "no");
                else if (r.response.result->type == cJSON_Array || r.response.result->type == cJSON_Object) {
                    char *j = cJSON_PrintUnformatted(r.response.result); fh_set(d, item->id, j); free(j);
                }
            }
            widget_destroy(d->sub_widget); d->sub_widget = NULL; cJSON_Delete(d->sub_req.params);
            d->mode = FHUB_BROWSING; d->base.dirty = true;
        }
        return event_result_handled();
    }
    switch (d->mode) {
    case FHUB_EDITING_MENU: { ForgeItem *item = fh_get_item(d, NULL); if (!item) break;
        switch (ev->code) {
            case KEY_ESC: d->mode = FHUB_BROWSING; d->base.dirty = true; return event_result_handled();
            case KEY_UP: if (d->edit_selected > 0) d->edit_selected--; d->base.dirty = true; return event_result_handled();
            case KEY_DOWN: if (d->edit_selected + 1 < item->choice_count) d->edit_selected++; d->base.dirty = true; return event_result_handled();
            case KEY_ENTER: fh_set(d, item->id, item->choices[d->edit_selected]); d->mode = FHUB_BROWSING; d->base.dirty = true; return event_result_handled();
            default: return event_result_unhandled();
        }
        break; }
    case FHUB_EDITING_INPUT: { ForgeItem *item = fh_get_item(d, NULL); if (!item) break;
        switch (ev->code) {
            case KEY_ESC: d->mode = FHUB_BROWSING; d->base.dirty = true; return event_result_handled();
            case KEY_ENTER: fh_set(d, item->id, d->edit_text); d->mode = FHUB_BROWSING; d->base.dirty = true; return event_result_handled();
            case KEY_CHAR: { int len = strlen(d->edit_text); d->edit_text = realloc(d->edit_text, len + 2); memmove(d->edit_text + d->edit_cursor + 1, d->edit_text + d->edit_cursor, len - d->edit_cursor + 1); d->edit_text[d->edit_cursor] = ev->ch; d->edit_cursor++; d->base.dirty = true; return event_result_handled(); }
            case KEY_BACKSPACE: if (d->edit_cursor > 0) { memmove(d->edit_text + d->edit_cursor - 1, d->edit_text + d->edit_cursor, strlen(d->edit_text + d->edit_cursor) + 1); d->edit_cursor--; d->base.dirty = true; } return event_result_handled();
            case KEY_LEFT: if (d->edit_cursor > 0) d->edit_cursor--; return event_result_handled();
            case KEY_RIGHT: if (d->edit_cursor < (int)strlen(d->edit_text)) d->edit_cursor++; return event_result_handled();
            case KEY_HOME: d->edit_cursor = 0; return event_result_handled();
            case KEY_END: d->edit_cursor = strlen(d->edit_text); return event_result_handled();
            default: return event_result_unhandled();
        }
        break; }
    case FHUB_EDITING_PASSWORD: { ForgeItem *item = fh_get_item(d, NULL); if (!item) break;
        switch (ev->code) {
            case KEY_ESC: d->mode = FHUB_BROWSING; d->base.dirty = true; return event_result_handled();
            case KEY_TAB: d->edit_pass_field = !d->edit_pass_field; d->base.dirty = true; return event_result_handled();
            case KEY_ENTER: if (strlen(d->edit_pass1) > 0 && strcmp(d->edit_pass1, d->edit_pass2) == 0) { fh_set(d, item->id, d->edit_pass1); d->mode = FHUB_BROWSING; d->base.dirty = true; } return event_result_handled();
            case KEY_CHAR: { char **t = d->edit_pass_field ? &d->edit_pass2 : &d->edit_pass1; int len = strlen(*t); *t = realloc(*t, len + 2); (*t)[len] = ev->ch; (*t)[len + 1] = '\0'; d->base.dirty = true; return event_result_handled(); }
            case KEY_BACKSPACE: { char **t = d->edit_pass_field ? &d->edit_pass2 : &d->edit_pass1; if (strlen(*t) > 0) (*t)[strlen(*t) - 1] = '\0'; d->base.dirty = true; return event_result_handled(); }
            default: return event_result_unhandled();
        }
        break; }
    case FHUB_EDITING_YESNO: { ForgeItem *item = fh_get_item(d, NULL); if (!item) break;
        switch (ev->code) {
            case KEY_ESC: d->mode = FHUB_BROWSING; d->base.dirty = true; return event_result_handled();
            case KEY_LEFT: case KEY_RIGHT: case KEY_TAB: d->edit_yes = !d->edit_yes; d->base.dirty = true; return event_result_handled();
            case KEY_ENTER: fh_set(d, item->id, d->edit_yes ? "yes" : "no"); d->mode = FHUB_BROWSING; d->base.dirty = true; return event_result_handled();
            case KEY_CHAR: if (ev->ch == 'y' || ev->ch == 'Y') { fh_set(d, item->id, "yes"); d->mode = FHUB_BROWSING; d->base.dirty = true; return event_result_handled(); } if (ev->ch == 'n' || ev->ch == 'N') { fh_set(d, item->id, "no"); d->mode = FHUB_BROWSING; d->base.dirty = true; return event_result_handled(); } return event_result_unhandled();
            default: return event_result_unhandled();
        }
        break; }
    case FHUB_EDITING_FILTER: { ForgeItem *item = fh_get_item(d, NULL); if (!item) break;
        switch (ev->code) {
            case KEY_ESC: d->mode = FHUB_BROWSING; d->base.dirty = true; return event_result_handled();
            case KEY_UP: if (d->edit_selected > 0) d->edit_selected--; d->base.dirty = true; return event_result_handled();
            case KEY_DOWN: if (d->edit_selected + 1 < d->edit_filtered_count) d->edit_selected++; d->base.dirty = true; return event_result_handled();
            case KEY_ENTER: if (d->edit_filtered_count > 0 && d->edit_selected < d->edit_filtered_count) { int orig = d->edit_filtered[d->edit_selected]; fh_set(d, item->id, item->choices[orig]); } d->mode = FHUB_BROWSING; d->base.dirty = true; return event_result_handled();
            case KEY_CHAR: { int len = strlen(d->edit_query); d->edit_query = realloc(d->edit_query, len + 2); d->edit_query[len] = ev->ch; d->edit_query[len + 1] = '\0'; d->edit_selected = 0; fh_update_filter(d, item->choices, item->choice_count); d->base.dirty = true; return event_result_handled(); }
            case KEY_BACKSPACE: if (strlen(d->edit_query) > 0) { d->edit_query[strlen(d->edit_query) - 1] = '\0'; d->edit_selected = 0; fh_update_filter(d, item->choices, item->choice_count); d->base.dirty = true; } return event_result_handled();
            default: return event_result_unhandled();
        }
        break; }
    case FHUB_EDITING_MULTISELECT: { ForgeItem *item = fh_get_item(d, NULL); if (!item) break;
        switch (ev->code) {
            case KEY_ESC: d->mode = FHUB_BROWSING; d->base.dirty = true; return event_result_handled();
            case KEY_UP: if (d->edit_selected > 0) d->edit_selected--; d->base.dirty = true; return event_result_handled();
            case KEY_DOWN: if (d->edit_selected + 1 < d->edit_filtered_count) d->edit_selected++; d->base.dirty = true; return event_result_handled();
            case KEY_ENTER: { cJSON *arr = cJSON_CreateArray(); for (int i = 0; i < item->choice_count; i++) if (d->edit_selected_set[i]) cJSON_AddItemToArray(arr, cJSON_CreateString(item->choices[i])); char *joined = cJSON_PrintUnformatted(arr); fh_set(d, item->id, joined); free(joined); cJSON_Delete(arr); d->mode = FHUB_BROWSING; d->base.dirty = true; return event_result_handled(); }
            case KEY_CHAR: if (ev->ch == ' ') { if (d->edit_filtered_count > 0 && d->edit_selected < d->edit_filtered_count) { int orig = d->edit_filtered[d->edit_selected]; d->edit_selected_set[orig] = !d->edit_selected_set[orig]; d->base.dirty = true; } } else { int len = strlen(d->edit_query); d->edit_query = realloc(d->edit_query, len + 2); d->edit_query[len] = ev->ch; d->edit_query[len + 1] = '\0'; d->edit_selected = 0; fh_update_filter(d, item->choices, item->choice_count); d->base.dirty = true; } return event_result_handled();
            case KEY_BACKSPACE: if (strlen(d->edit_query) > 0) { d->edit_query[strlen(d->edit_query) - 1] = '\0'; d->edit_selected = 0; fh_update_filter(d, item->choices, item->choice_count); d->base.dirty = true; } return event_result_handled();
            default: return event_result_unhandled();
        }
        break; }
    default: break;
    }
    return event_result_unhandled();
}

static EventResult fh_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    ForgeHubData *d = (ForgeHubData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    if (d->mode == FHUB_EDITING_SUB_WIDGET && d->sub_widget) return fh_handle_edit_event(d, ev);
    if (d->mode >= FHUB_EDITING_MENU && d->mode <= FHUB_EDITING_MULTISELECT) return fh_handle_edit_event(d, ev);
    if (d->mode == FHUB_VIEW_LOG || d->mode == FHUB_VIEW_QUEUE) {
        if (ev->code == KEY_ESC) { d->mode = FHUB_BROWSING; d->base.dirty = true; return event_result_handled(); }
        if (d->mode == FHUB_VIEW_QUEUE) {
            if (ev->code == KEY_F5) return event_result_response((WidgetResponse){ .result = cJSON_CreateString("retry_failed"), .cancelled = false });
            if (ev->code == KEY_F4) return event_result_response((WidgetResponse){ .result = cJSON_CreateString("skip_package"), .cancelled = false });
        }
        return event_result_handled();
    }
    if (d->mode == FHUB_CONFIRM_QUIT) {
        if (ev->code == KEY_CHAR && (ev->ch == 'y' || ev->ch == 'Y'))
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        d->mode = FHUB_BROWSING; d->base.dirty = true; return event_result_handled();
    }
    if (d->mode == FHUB_CONFIRM_PROCEED) {
        if (ev->code == KEY_CHAR && (ev->ch == 'y' || ev->ch == 'Y')) {
            cJSON *result = cJSON_CreateObject();
            for (int i = 0; i < d->val_count; i++)
                cJSON_AddStringToObject(result, d->keys[i], d->vals[i]);
            return event_result_response((WidgetResponse){ .result = result, .cancelled = false });
        }
        d->mode = FHUB_BROWSING; d->base.dirty = true; return event_result_handled();
    }
    switch (ev->code) {
        case KEY_ESC: d->mode = FHUB_CONFIRM_QUIT; d->base.dirty = true; return event_result_handled();
        case KEY_UP: if (d->item_idx > 0) d->item_idx--; d->base.dirty = true; return event_result_handled();
        case KEY_DOWN: { int vc = fh_visible_count(d); if (d->item_idx + 1 < vc) d->item_idx++; d->base.dirty = true; return event_result_handled(); }
        case KEY_LEFT: d->cat_idx = d->cat_idx > 0 ? d->cat_idx - 1 : d->cat_count - 1; d->item_idx = 0; d->base.dirty = true; return event_result_handled();
        case KEY_RIGHT: case KEY_TAB: d->cat_idx = d->cat_idx + 1 < d->cat_count ? d->cat_idx + 1 : 0; d->item_idx = 0; d->base.dirty = true; return event_result_handled();
        case KEY_ENTER: { ForgeItem *item = fh_get_item(d, NULL); if (!item) return event_result_handled(); fh_enter_edit(d, item); return event_result_handled(); }
        case KEY_F2: d->mode = FHUB_VIEW_LOG; d->base.dirty = true; return event_result_handled();
        case KEY_F3: d->mode = FHUB_VIEW_QUEUE; d->base.dirty = true; return event_result_handled();
        case KEY_F5: return event_result_response((WidgetResponse){ .result = cJSON_CreateString("retry_failed"), .cancelled = false });
        case KEY_F10: return event_result_response((WidgetResponse){ .result = cJSON_CreateString("abort_build"), .cancelled = false });
        default: {
            if (ev->code >= KEY_F1 && ev->code <= KEY_F12) {
                int f = ev->code - KEY_F1;
                if (f < d->action_count) {
                    if (strcmp(d->actions[f], "Proceed") == 0) { d->mode = FHUB_CONFIRM_PROCEED; d->base.dirty = true; return event_result_handled(); }
                    return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->actions[f]), .cancelled = false });
                }
            }
            return event_result_unhandled();
        }
    }
}

static void fh_destroy(Widget *self) {
    ForgeHubData *d = (ForgeHubData *)(self + 1);
    free(d->title); free(d->build_stage);
    for (int i = 0; i < d->cat_count; i++) {
        free(d->categories[i].id); free(d->categories[i].label); free(d->categories[i].summary_template);
        for (int j = 0; j < d->categories[i].item_count; j++) {
            ForgeItem *item = &d->categories[i].items[j];
            free(item->id); free(item->label); free(item->value); free(item->widget);
            for (int k = 0; k < item->choice_count; k++) { free(item->choices[k]); }
            free(item->choices);
            free(item->placeholder); free(item->message); free(item->display);
            for (int k = 0; k < item->visible_if.count; k++) { free(item->visible_if.keys[k]); free(item->visible_if.vals[k]); }
            free(item->visible_if.keys); free(item->visible_if.vals);
        }
        free(d->categories[i].items);
    }
    free(d->categories);
    for (int i = 0; i < d->action_count; i++) { free(d->actions[i]); }
    free(d->actions);
    for (int i = 0; i < d->val_count; i++) { free(d->keys[i]); free(d->vals[i]); }
    free(d->keys); free(d->vals);
    free(d->edit_text); free(d->edit_pass1); free(d->edit_pass2);
    free(d->edit_query); free(d->edit_filtered); free(d->edit_selected_set);
    for (int i = 0; i < d->build_queue_count; i++) { free(d->build_queue[i]); }
    free(d->build_queue);
    if (d->sub_widget) { widget_destroy(d->sub_widget); cJSON_Delete(d->sub_req.params); }
}

Widget *forge_hub_factory(const WidgetRequest *req) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(ForgeHubData));
    ForgeHubData data;
    memset(&data, 0, sizeof(data));
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    data.title = strdup(t && t->valuestring ? t->valuestring : "ForgeLFS");

    cJSON *cats = cJSON_GetObjectItem(req->params, "categories");
    if (cats && cats->type == cJSON_Array) {
        data.cat_count = cJSON_GetArraySize(cats);
        data.categories = calloc(data.cat_count, sizeof(ForgeCategory));
        int ci = 0; cJSON *cat_val;
        cJSON_ArrayForEach(cat_val, cats) {
            cJSON *cid = cJSON_GetObjectItem(cat_val, "id");
            cJSON *clabel = cJSON_GetObjectItem(cat_val, "label");
            cJSON *ctmpl = cJSON_GetObjectItem(cat_val, "summary_template");
            data.categories[ci].id = cid && cid->valuestring ? strdup(cid->valuestring) : strdup("");
            data.categories[ci].label = clabel && clabel->valuestring ? strdup(clabel->valuestring) : strdup("");
            data.categories[ci].summary_template = ctmpl && ctmpl->valuestring ? strdup(ctmpl->valuestring) : strdup("");
            cJSON *items_arr = cJSON_GetObjectItem(cat_val, "items");
            data.categories[ci].item_count = items_arr ? cJSON_GetArraySize(items_arr) : 0;
            data.categories[ci].items = calloc(data.categories[ci].item_count, sizeof(ForgeItem));
            int ii = 0; cJSON *item_val;
            cJSON_ArrayForEach(item_val, items_arr) {
                ForgeItem *item = &data.categories[ci].items[ii];
                cJSON *iid = cJSON_GetObjectItem(item_val, "id"); item->id = iid && iid->valuestring ? strdup(iid->valuestring) : strdup("");
                cJSON *il = cJSON_GetObjectItem(item_val, "label"); item->label = il && il->valuestring ? strdup(il->valuestring) : strdup("");
                cJSON *iv = cJSON_GetObjectItem(item_val, "value"); item->value = iv && iv->valuestring ? strdup(iv->valuestring) : strdup("");
                cJSON *iw = cJSON_GetObjectItem(item_val, "widget"); item->widget = iw && iw->valuestring ? strdup(iw->valuestring) : strdup("menu");
                cJSON *ich = cJSON_GetObjectItem(item_val, "choices");
                item->choice_count = ich ? cJSON_GetArraySize(ich) : 0;
                item->choices = item->choice_count > 0 ? malloc(item->choice_count * sizeof(char *)) : NULL;
                for (int j = 0; j < item->choice_count; j++)
                    item->choices[j] = strdup(cJSON_GetArrayItem(ich, j)->valuestring);
                cJSON *ip = cJSON_GetObjectItem(item_val, "placeholder"); item->placeholder = ip && ip->valuestring ? strdup(ip->valuestring) : strdup("");
                cJSON *im = cJSON_GetObjectItem(item_val, "message"); item->message = im && im->valuestring ? strdup(im->valuestring) : strdup("");
                cJSON *idisp = cJSON_GetObjectItem(item_val, "display"); item->display = idisp && idisp->valuestring ? strdup(idisp->valuestring) : NULL;
                cJSON *vi = cJSON_GetObjectItem(item_val, "visible_if"); item->visible_if.keys = NULL; item->visible_if.vals = NULL; item->visible_if.count = 0;
                if (vi && vi->type == cJSON_Object) {
                    item->visible_if.count = cJSON_GetArraySize(vi);
                    item->visible_if.keys = malloc(item->visible_if.count * sizeof(char *));
                    item->visible_if.vals = malloc(item->visible_if.count * sizeof(char *));
                    cJSON *child = vi->child; int k = 0;
                    while (child) { item->visible_if.keys[k] = strdup(child->string); item->visible_if.vals[k] = child->valuestring ? strdup(child->valuestring) : strdup(""); k++; child = child->next; }
                }
                fh_set(&data, item->id, item->value);
                ii++;
            }
            ci++;
        }
    }
    cJSON *acts = cJSON_GetObjectItem(req->params, "actions");
    if (acts && acts->type == cJSON_Array) {
        data.action_count = cJSON_GetArraySize(acts);
        data.actions = malloc(data.action_count * sizeof(char *));
        for (int i = 0; i < data.action_count; i++)
            data.actions[i] = strdup(cJSON_GetArrayItem(acts, i)->valuestring);
    }
    cJSON *build = cJSON_GetObjectItem(req->params, "build_state");
    if (build) {
        cJSON *stage = cJSON_GetObjectItem(build, "stage");
        cJSON *current = cJSON_GetObjectItem(build, "current");
        cJSON *total = cJSON_GetObjectItem(build, "total");
        cJSON *failed = cJSON_GetObjectItem(build, "failed");
        cJSON *queue = cJSON_GetObjectItem(build, "queue");
        data.build_stage = stage && stage->valuestring ? strdup(stage->valuestring) : NULL;
        data.build_current = current ? current->valueint : 0;
        data.build_total = total ? total->valueint : 0;
        data.build_failed = failed ? failed->valueint : 0;
        if (queue && queue->type == cJSON_Array) {
            data.build_queue_count = cJSON_GetArraySize(queue);
            data.build_queue = malloc(data.build_queue_count * sizeof(char *));
            for (int i = 0; i < data.build_queue_count; i++)
                data.build_queue[i] = strdup(cJSON_GetArrayItem(queue, i)->valuestring);
        }
    }
    widget_base_init(w, &data, sizeof(ForgeHubData), fh_render, fh_handle_event, fh_destroy);
    return w;
}