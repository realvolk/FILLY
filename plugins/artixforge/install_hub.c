#include "../../filly-core/widget.h"
#include "../../filly-core/render.h"
#include "../../filly-protocol/protocol.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

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
    HUB_EDITING_MENU,
    HUB_EDITING_INPUT,
    HUB_EDITING_PASSWORD,
    HUB_EDITING_YESNO,
    HUB_EDITING_FILTER,
    HUB_EDITING_MULTISELECT,
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

    int edit_selected;
    int edit_cursor;
    char *edit_text;
    char *edit_pass1;
    char *edit_pass2;
    bool edit_pass_field;
    bool edit_yes;
    char *edit_query;
    int *edit_filtered;
    int edit_filtered_count;
    bool *edit_selected_set;
    int edit_min_sel;
    int edit_max_sel;

    bool dirty;
} HubData;

static char *hub_get(HubData *d, const char *key) {
    for (int i = 0; i < d->val_count; i++)
        if (strcmp(d->keys[i], key) == 0) return d->vals[i];
    return NULL;
}

static void hub_set(HubData *d, const char *key, const char *value) {
    for (int i = 0; i < d->val_count; i++) {
        if (strcmp(d->keys[i], key) == 0) {
            free(d->vals[i]);
            d->vals[i] = strdup(value);
            return;
        }
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

static HubItem *hub_get_item(HubData *d, int *out_vi) {
    HubCategory *cat = &d->categories[d->cat_idx];
    int vi = 0;
    for (int i = 0; i < cat->item_count; i++) {
        if (!hub_visible(d, &cat->items[i])) continue;
        if (vi == d->item_idx) {
            if (out_vi) *out_vi = vi;
            return &cat->items[i];
        }
        vi++;
    }
    return NULL;
}

static int hub_visible_count(HubData *d) {
    HubCategory *cat = &d->categories[d->cat_idx];
    int vc = 0;
    for (int i = 0; i < cat->item_count; i++)
        if (hub_visible(d, &cat->items[i])) vc++;
    return vc;
}

static char *get_disks(void) {
    FILE *fp = popen("lsblk -dpno NAME,SIZE,MODEL -e 7 2>/dev/null", "r");
    if (!fp) return strdup("[]");
    char buf[256];
    char *result = strdup("[");
    bool first = true;
    while (fgets(buf, sizeof(buf), fp)) {
        buf[strcspn(buf, "\n")] = 0;
        char label[512];
        snprintf(label, sizeof(label), "%s", buf);
        char *escaped = malloc(strlen(label) * 2 + 3);
        char *p = escaped;
        *p++ = '"';
        for (char *c = label; *c; c++) {
            if (*c == '"' || *c == '\\') *p++ = '\\';
            *p++ = *c;
        }
        *p++ = '"';
        *p = '\0';
        if (!first) {
            char *tmp = malloc(strlen(result) + strlen(escaped) + 2);
            sprintf(tmp, "%s,%s", result, escaped);
            free(result);
            result = tmp;
        } else {
            char *tmp = malloc(strlen(result) + strlen(escaped) + 1);
            sprintf(tmp, "%s%s", result, escaped);
            free(result);
            result = tmp;
        }
        first = false;
        free(escaped);
    }
    pclose(fp);
    char *tmp = malloc(strlen(result) + 2);
    sprintf(tmp, "%s]", result);
    free(result);
    return tmp;
}

static void hub_update_filter(HubData *d, char **choices, int count) {
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
    if (d->edit_selected >= d->edit_filtered_count)
        d->edit_selected = d->edit_filtered_count > 0 ? d->edit_filtered_count - 1 : 0;
}

static void hub_enter_edit(HubData *d, HubItem *item) {
    char *current = hub_get(d, item->id);
    if (!current) current = item->value;

    if (item->disk_picker) {
        item->widget = strdup("menu");
        char *disks_json = get_disks();
        cJSON *disks_arr = cJSON_Parse(disks_json);
        free(disks_json);
        if (disks_arr && disks_arr->type == cJSON_Array) {
            free(item->choices);
            item->choice_count = cJSON_GetArraySize(disks_arr);
            item->choices = malloc(item->choice_count * sizeof(char *));
            for (int i = 0; i < item->choice_count; i++)
                item->choices[i] = strdup(cJSON_GetArrayItem(disks_arr, i)->valuestring);
        }
        if (disks_arr) cJSON_Delete(disks_arr);
        current = item->choices && item->choice_count > 0 ? item->choices[0] : "";
    }

    if (strcmp(item->widget, "menu") == 0 || strcmp(item->widget, "filter") == 0) {
        d->edit_selected = 0;
        for (int i = 0; i < item->choice_count; i++) {
            if (strcmp(item->choices[i], current) == 0) {
                d->edit_selected = i;
                break;
            }
        }
        if (strcmp(item->widget, "filter") == 0) {
            free(d->edit_query);
            d->edit_query = strdup("");
            hub_update_filter(d, item->choices, item->choice_count);
            d->mode = HUB_EDITING_FILTER;
        } else {
            d->mode = HUB_EDITING_MENU;
        }
    } else if (strcmp(item->widget, "input") == 0) {
        free(d->edit_text);
        d->edit_text = strdup(current);
        d->edit_cursor = strlen(d->edit_text);
        d->mode = HUB_EDITING_INPUT;
    } else if (strcmp(item->widget, "password") == 0 || strcmp(item->widget, "password_confirm") == 0) {
        free(d->edit_pass1);
        free(d->edit_pass2);
        d->edit_pass1 = strdup("");
        d->edit_pass2 = strdup("");
        d->edit_pass_field = false;
        d->mode = HUB_EDITING_PASSWORD;
    } else if (strcmp(item->widget, "yesno") == 0) {
        d->edit_yes = (strcmp(current, "yes") == 0);
        d->mode = HUB_EDITING_YESNO;
    } else if (strcmp(item->widget, "multiselect") == 0) {
        free(d->edit_query);
        d->edit_query = strdup("");
        free(d->edit_selected_set);
        d->edit_selected_set = calloc(item->choice_count, sizeof(bool));
        d->edit_min_sel = 0;
        d->edit_max_sel = item->choice_count;
        hub_update_filter(d, item->choices, item->choice_count);
        d->edit_selected = 0;
        d->mode = HUB_EDITING_MULTISELECT;
    }
    d->dirty = true;
}

static void hub_render_overlay(HubData *d, Rect area, RenderTree *out) {
    int ow = (int)(area.w * 0.55f);
    int oh = (int)(area.h * 0.60f);
    if (ow > area.w - 4) ow = area.w - 4;
    if (oh > area.h - 4) oh = area.h - 4;
    int ox = (area.w - ow) / 2;
    int oy = (area.h - oh) / 2;

    RenderTree *children = NULL;
    int child_count = 0;

    HubItem *item = hub_get_item(d, NULL);

    switch (d->mode) {
    case HUB_EDITING_MENU: {
        child_count = 3;
        children = calloc(child_count, sizeof(RenderTree));
        children[0].type = RNODE_TEXT;
        children[0].rect = rect_new(1, 0, ow - 2, 1);
        children[0].text.content = item ? strdup(item->label) : strdup("");
        children[0].text.align = ALIGN_CENTER;
        children[0].text.style = textstyle_selected();

        children[1].type = RNODE_LIST;
        children[1].rect = rect_new(1, 1, ow - 2, oh - 4);
        children[1].list.item_count = item ? item->choice_count : 0;
        children[1].list.items = item ? malloc(item->choice_count * sizeof(ListItem)) : NULL;
        if (item) for (int i = 0; i < item->choice_count; i++)
            children[1].list.items[i] = listitem_new(item->choices[i]);
        children[1].list.selected = d->edit_selected;
        children[1].list.highlight = textstyle_selected();

        children[2].type = RNODE_TEXT;
        children[2].rect = rect_new(1, oh - 3, ow - 2, 1);
        children[2].text.content = strdup("Up/Down:move  Enter:select  Esc:cancel");
        children[2].text.align = ALIGN_CENTER;
        children[2].text.style = textstyle_muted();
        break;
    }
    case HUB_EDITING_INPUT: {
        child_count = 3;
        children = calloc(child_count, sizeof(RenderTree));
        children[0].type = RNODE_TEXT;
        children[0].rect = rect_new(1, 0, ow - 2, 1);
        children[0].text.content = item ? strdup(item->label) : strdup("");
        children[0].text.align = ALIGN_CENTER;
        children[0].text.style = textstyle_selected();

        children[1].type = RNODE_INPUT;
        children[1].rect = rect_new(1, 2, ow - 2, 1);
        children[1].input.text = d->edit_text ? strdup(d->edit_text) : strdup("");
        children[1].input.cursor = d->edit_cursor;
        children[1].input.placeholder = strdup("");
        children[1].input.masked = false;

        children[2].type = RNODE_TEXT;
        children[2].rect = rect_new(1, oh - 2, ow - 2, 1);
        children[2].text.content = strdup("Enter:confirm  Esc:cancel");
        children[2].text.align = ALIGN_CENTER;
        children[2].text.style = textstyle_muted();
        break;
    }
    case HUB_EDITING_PASSWORD: {
        child_count = 4;
        children = calloc(child_count, sizeof(RenderTree));
        children[0].type = RNODE_TEXT;
        children[0].rect = rect_new(1, 0, ow - 2, 1);
        children[0].text.content = item ? strdup(item->label) : strdup("");
        children[0].text.align = ALIGN_CENTER;
        children[0].text.style = textstyle_selected();

        char mask1[128] = {0};
        for (int j = 0; j < (int)strlen(d->edit_pass1) && j < 127; j++) mask1[j] = '*';
        children[1].type = RNODE_TEXT;
        children[1].rect = rect_new(1, 2, ow - 2, 1);
        char buf[256];
        snprintf(buf, sizeof(buf), "Password: %s", mask1);
        children[1].text.content = strdup(buf);
        children[1].text.align = ALIGN_LEFT;
        children[1].text.style = d->edit_pass_field ? textstyle_normal() : textstyle_selected();

        char mask2[128] = {0};
        for (int j = 0; j < (int)strlen(d->edit_pass2) && j < 127; j++) mask2[j] = '*';
        children[2].type = RNODE_TEXT;
        children[2].rect = rect_new(1, 3, ow - 2, 1);
        snprintf(buf, sizeof(buf), "Confirm:  %s", mask2);
        children[2].text.content = strdup(buf);
        children[2].text.align = ALIGN_LEFT;
        children[2].text.style = d->edit_pass_field ? textstyle_selected() : textstyle_normal();

        children[3].type = RNODE_TEXT;
        children[3].rect = rect_new(1, oh - 2, ow - 2, 1);
        children[3].text.content = strdup("Tab:next  Enter:submit  Esc:cancel");
        children[3].text.align = ALIGN_CENTER;
        children[3].text.style = textstyle_muted();
        break;
    }
    case HUB_EDITING_YESNO: {
        child_count = 3;
        children = calloc(child_count, sizeof(RenderTree));
        children[0].type = RNODE_TEXT;
        children[0].rect = rect_new(1, 0, ow - 2, 1);
        children[0].text.content = item ? strdup(item->label) : strdup("");
        children[0].text.align = ALIGN_CENTER;
        children[0].text.style = textstyle_selected();

        char yesno_text[64];
        snprintf(yesno_text, sizeof(yesno_text), "[ %s ]  [ %s ]",
            d->edit_yes ? "Yes" : "yes", d->edit_yes ? "no" : "No");
        children[1].type = RNODE_TEXT;
        children[1].rect = rect_new(1, 3, ow - 2, 1);
        children[1].text.content = strdup(yesno_text);
        children[1].text.align = ALIGN_CENTER;
        children[1].text.style = textstyle_accent();

        children[2].type = RNODE_TEXT;
        children[2].rect = rect_new(1, oh - 2, ow - 2, 1);
        children[2].text.content = strdup("Left/Right:choose  Enter:confirm  y/n:quick  Esc:cancel");
        children[2].text.align = ALIGN_CENTER;
        children[2].text.style = textstyle_muted();
        break;
    }
    case HUB_EDITING_FILTER: {
        child_count = 4;
        children = calloc(child_count, sizeof(RenderTree));
        children[0].type = RNODE_TEXT;
        children[0].rect = rect_new(1, 0, ow - 2, 1);
        children[0].text.content = item ? strdup(item->label) : strdup("");
        children[0].text.align = ALIGN_CENTER;
        children[0].text.style = textstyle_selected();

        children[1].type = RNODE_INPUT;
        children[1].rect = rect_new(1, 1, ow - 2, 1);
        char query_display[256];
        snprintf(query_display, sizeof(query_display), "> %s", d->edit_query ? d->edit_query : "");
        children[1].input.text = strdup(query_display);
        children[1].input.cursor = strlen(query_display);
        children[1].input.placeholder = strdup("Type to filter...");
        children[1].input.masked = false;

        children[2].type = RNODE_LIST;
        children[2].rect = rect_new(1, 2, ow - 2, oh - 5);
        children[2].list.item_count = d->edit_filtered_count;
        children[2].list.items = malloc(d->edit_filtered_count * sizeof(ListItem));
        for (int i = 0; i < d->edit_filtered_count; i++)
            children[2].list.items[i] = listitem_new(item->choices[d->edit_filtered[i]]);
        children[2].list.selected = d->edit_selected;
        children[2].list.highlight = textstyle_selected();

        children[3].type = RNODE_TEXT;
        children[3].rect = rect_new(1, oh - 2, ow - 2, 1);
        children[3].text.content = strdup("Type:filter  Up/Down:move  Enter:select  Esc:cancel");
        children[3].text.align = ALIGN_CENTER;
        children[3].text.style = textstyle_muted();
        break;
    }
    case HUB_EDITING_MULTISELECT: {
        child_count = 4;
        children = calloc(child_count, sizeof(RenderTree));
        children[0].type = RNODE_TEXT;
        children[0].rect = rect_new(1, 0, ow - 2, 1);
        children[0].text.content = item ? strdup(item->label) : strdup("");
        children[0].text.align = ALIGN_CENTER;
        children[0].text.style = textstyle_selected();

        children[1].type = RNODE_INPUT;
        children[1].rect = rect_new(1, 1, ow - 2, 1);
        char qd[256];
        snprintf(qd, sizeof(qd), "> %s", d->edit_query ? d->edit_query : "");
        children[1].input.text = strdup(qd);
        children[1].input.cursor = strlen(qd);
        children[1].input.placeholder = strdup("Type to filter...");
        children[1].input.masked = false;

        children[2].type = RNODE_LIST;
        children[2].rect = rect_new(1, 2, ow - 2, oh - 5);
        children[2].list.item_count = d->edit_filtered_count;
        children[2].list.items = malloc(d->edit_filtered_count * sizeof(ListItem));
        for (int i = 0; i < d->edit_filtered_count; i++) {
            int orig = d->edit_filtered[i];
            char label[512];
            snprintf(label, sizeof(label), "%s %s",
                d->edit_selected_set[orig] ? "[x]" : "[ ]", item->choices[orig]);
            children[2].list.items[i] = listitem_new(label);
        }
        children[2].list.selected = d->edit_selected;
        children[2].list.highlight = textstyle_selected();

        int sel_count = 0;
        for (int i = 0; i < item->choice_count; i++)
            if (d->edit_selected_set[i]) sel_count++;
        char footer[256];
        snprintf(footer, sizeof(footer), "%d selected  Space:toggle  Enter:confirm  Esc:cancel", sel_count);
        children[3].type = RNODE_TEXT;
        children[3].rect = rect_new(1, oh - 2, ow - 2, 1);
        children[3].text.content = strdup(footer);
        children[3].text.align = ALIGN_CENTER;
        children[3].text.style = textstyle_muted();
        break;
    }
    default: break;
    }

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(ox, oy, ow, oh);
    out->container.bg = strdup("0");
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = child_count;
}

static void hub_render(Widget *self, Rect area, RenderTree *out) {
    HubData *d = (HubData *)(self + 1);
    memset(out, 0, sizeof(*out));

    if (d->mode >= HUB_EDITING_MENU && d->mode <= HUB_EDITING_MULTISELECT) {
        hub_render_overlay(d, area, out);
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
    int vis_count = hub_visible_count(d);

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

static EventResult hub_handle_edit_event(HubData *d, Event *ev) {
    if (ev->type != EVENT_KEY) return event_result_unhandled();

    switch (d->mode) {
    case HUB_EDITING_MENU: {
        HubItem *item = hub_get_item(d, NULL);
        if (!item) break;
        switch (ev->code) {
            case KEY_ESC: d->mode = HUB_BROWSING; d->dirty = true; return event_result_handled();
            case KEY_UP: if (d->edit_selected > 0) d->edit_selected--; d->dirty = true; return event_result_handled();
            case KEY_DOWN: if (d->edit_selected + 1 < item->choice_count) d->edit_selected++; d->dirty = true; return event_result_handled();
            case KEY_ENTER:
                hub_set(d, item->id, item->choices[d->edit_selected]);
                d->mode = HUB_BROWSING; d->dirty = true; return event_result_handled();
            default: return event_result_unhandled();
        }
        break;
    }
    case HUB_EDITING_INPUT: {
        HubItem *item = hub_get_item(d, NULL);
        if (!item) break;
        switch (ev->code) {
            case KEY_ESC: d->mode = HUB_BROWSING; d->dirty = true; return event_result_handled();
            case KEY_ENTER:
                hub_set(d, item->id, d->edit_text);
                d->mode = HUB_BROWSING; d->dirty = true; return event_result_handled();
            case KEY_CHAR: {
                int len = strlen(d->edit_text);
                d->edit_text = realloc(d->edit_text, len + 2);
                memmove(d->edit_text + d->edit_cursor + 1, d->edit_text + d->edit_cursor, len - d->edit_cursor + 1);
                d->edit_text[d->edit_cursor] = ev->ch;
                d->edit_cursor++; d->dirty = true; return event_result_handled();
            }
            case KEY_BACKSPACE:
                if (d->edit_cursor > 0) {
                    memmove(d->edit_text + d->edit_cursor - 1, d->edit_text + d->edit_cursor, strlen(d->edit_text + d->edit_cursor) + 1);
                    d->edit_cursor--; d->dirty = true;
                } return event_result_handled();
            case KEY_LEFT: if (d->edit_cursor > 0) d->edit_cursor--; return event_result_handled();
            case KEY_RIGHT: if (d->edit_cursor < (int)strlen(d->edit_text)) d->edit_cursor++; return event_result_handled();
            case KEY_HOME: d->edit_cursor = 0; return event_result_handled();
            case KEY_END: d->edit_cursor = strlen(d->edit_text); return event_result_handled();
            default: return event_result_unhandled();
        }
        break;
    }
    case HUB_EDITING_PASSWORD: {
        HubItem *item = hub_get_item(d, NULL);
        if (!item) break;
        switch (ev->code) {
            case KEY_ESC: d->mode = HUB_BROWSING; d->dirty = true; return event_result_handled();
            case KEY_TAB: d->edit_pass_field = !d->edit_pass_field; d->dirty = true; return event_result_handled();
            case KEY_ENTER:
                if (strlen(d->edit_pass1) > 0 && strcmp(d->edit_pass1, d->edit_pass2) == 0) {
                    hub_set(d, item->id, d->edit_pass1);
                    d->mode = HUB_BROWSING; d->dirty = true;
                } return event_result_handled();
            case KEY_CHAR: {
                char **target = d->edit_pass_field ? &d->edit_pass2 : &d->edit_pass1;
                int len = strlen(*target);
                *target = realloc(*target, len + 2);
                (*target)[len] = ev->ch;
                (*target)[len + 1] = '\0';
                d->dirty = true; return event_result_handled();
            }
            case KEY_BACKSPACE: {
                char **target = d->edit_pass_field ? &d->edit_pass2 : &d->edit_pass1;
                if (strlen(*target) > 0) (*target)[strlen(*target) - 1] = '\0';
                d->dirty = true; return event_result_handled();
            }
            default: return event_result_unhandled();
        }
        break;
    }
    case HUB_EDITING_YESNO: {
        HubItem *item = hub_get_item(d, NULL);
        if (!item) break;
        switch (ev->code) {
            case KEY_ESC: d->mode = HUB_BROWSING; d->dirty = true; return event_result_handled();
            case KEY_LEFT: case KEY_RIGHT: case KEY_TAB:
                d->edit_yes = !d->edit_yes; d->dirty = true; return event_result_handled();
            case KEY_ENTER:
                hub_set(d, item->id, d->edit_yes ? "yes" : "no");
                d->mode = HUB_BROWSING; d->dirty = true; return event_result_handled();
            case KEY_CHAR:
                if (ev->ch == 'y' || ev->ch == 'Y') {
                    hub_set(d, item->id, "yes");
                    d->mode = HUB_BROWSING; d->dirty = true; return event_result_handled();
                }
                if (ev->ch == 'n' || ev->ch == 'N') {
                    hub_set(d, item->id, "no");
                    d->mode = HUB_BROWSING; d->dirty = true; return event_result_handled();
                }
                return event_result_unhandled();
            default: return event_result_unhandled();
        }
        break;
    }
    case HUB_EDITING_FILTER: {
        HubItem *item = hub_get_item(d, NULL);
        if (!item) break;
        switch (ev->code) {
            case KEY_ESC: d->mode = HUB_BROWSING; d->dirty = true; return event_result_handled();
            case KEY_UP: if (d->edit_selected > 0) d->edit_selected--; d->dirty = true; return event_result_handled();
            case KEY_DOWN: if (d->edit_selected + 1 < d->edit_filtered_count) d->edit_selected++; d->dirty = true; return event_result_handled();
            case KEY_ENTER:
                if (d->edit_filtered_count > 0 && d->edit_selected < d->edit_filtered_count) {
                    int orig = d->edit_filtered[d->edit_selected];
                    hub_set(d, item->id, item->choices[orig]);
                }
                d->mode = HUB_BROWSING; d->dirty = true; return event_result_handled();
            case KEY_CHAR: {
                int len = strlen(d->edit_query);
                d->edit_query = realloc(d->edit_query, len + 2);
                d->edit_query[len] = ev->ch;
                d->edit_query[len + 1] = '\0';
                d->edit_selected = 0;
                hub_update_filter(d, item->choices, item->choice_count);
                d->dirty = true; return event_result_handled();
            }
            case KEY_BACKSPACE:
                if (strlen(d->edit_query) > 0) {
                    d->edit_query[strlen(d->edit_query) - 1] = '\0';
                    d->edit_selected = 0;
                    hub_update_filter(d, item->choices, item->choice_count);
                    d->dirty = true;
                } return event_result_handled();
            default: return event_result_unhandled();
        }
        break;
    }
    case HUB_EDITING_MULTISELECT: {
        HubItem *item = hub_get_item(d, NULL);
        if (!item) break;
        switch (ev->code) {
            case KEY_ESC: d->mode = HUB_BROWSING; d->dirty = true; return event_result_handled();
            case KEY_UP: if (d->edit_selected > 0) d->edit_selected--; d->dirty = true; return event_result_handled();
            case KEY_DOWN: if (d->edit_selected + 1 < d->edit_filtered_count) d->edit_selected++; d->dirty = true; return event_result_handled();
            case KEY_ENTER: {
                cJSON *arr = cJSON_CreateArray();
                for (int i = 0; i < item->choice_count; i++)
                    if (d->edit_selected_set[i]) cJSON_AddItemToArray(arr, cJSON_CreateString(item->choices[i]));
                char *joined = cJSON_PrintUnformatted(arr);
                hub_set(d, item->id, joined);
                free(joined);
                cJSON_Delete(arr);
                d->mode = HUB_BROWSING; d->dirty = true; return event_result_handled();
            }
            case KEY_CHAR: {
                if (ev->ch == ' ') {
                    if (d->edit_filtered_count > 0 && d->edit_selected < d->edit_filtered_count) {
                        int orig = d->edit_filtered[d->edit_selected];
                        d->edit_selected_set[orig] = !d->edit_selected_set[orig];
                        d->dirty = true;
                    }
                } else {
                    int len = strlen(d->edit_query);
                    d->edit_query = realloc(d->edit_query, len + 2);
                    d->edit_query[len] = ev->ch;
                    d->edit_query[len + 1] = '\0';
                    d->edit_selected = 0;
                    hub_update_filter(d, item->choices, item->choice_count);
                    d->dirty = true;
                }
                return event_result_handled();
            }
            case KEY_BACKSPACE:
                if (strlen(d->edit_query) > 0) {
                    d->edit_query[strlen(d->edit_query) - 1] = '\0';
                    d->edit_selected = 0;
                    hub_update_filter(d, item->choices, item->choice_count);
                    d->dirty = true;
                } return event_result_handled();
            default: return event_result_unhandled();
        }
        break;
    }
    default: break;
    }
    return event_result_unhandled();
}

static EventResult hub_handle_event(Widget *self, Event *ev, Backend *backend) {
    HubData *d = (HubData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();

    if (d->mode >= HUB_EDITING_MENU && d->mode <= HUB_EDITING_MULTISELECT)
        return hub_handle_edit_event(d, ev);

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

    switch (ev->code) {
        case KEY_ESC: d->mode = HUB_CONFIRM_QUIT; d->dirty = true; return event_result_handled();
        case KEY_UP: if (d->item_idx > 0) d->item_idx--; d->dirty = true; return event_result_handled();
        case KEY_DOWN: {
            int vc = hub_visible_count(d);
            if (d->item_idx + 1 < vc) d->item_idx++;
            d->dirty = true;
            return event_result_handled();
        }
        case KEY_LEFT: if (d->cat_idx > 0) d->cat_idx--; else d->cat_idx = d->cat_count - 1; d->item_idx = 0; d->dirty = true; return event_result_handled();
        case KEY_RIGHT: case KEY_TAB: if (d->cat_idx + 1 < d->cat_count) d->cat_idx++; else d->cat_idx = 0; d->item_idx = 0; d->dirty = true; return event_result_handled();
        case KEY_ENTER: {
            HubItem *item = hub_get_item(d, NULL);
            if (!item) return event_result_handled();
            hub_enter_edit(d, item);
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

static bool hub_is_dirty(Widget *self) { return ((HubData *)(self + 1))->dirty; }
static void hub_clear_dirty(Widget *self) { ((HubData *)(self + 1))->dirty = false; }

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
    free(d->edit_text); free(d->edit_pass1); free(d->edit_pass2);
    free(d->edit_query); free(d->edit_filtered); free(d->edit_selected_set);
}

Widget *install_hub_factory(const WidgetRequest *req) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(HubData));
    w->vtable.render = hub_render;
    w->vtable.handle_event = hub_handle_event;
    w->vtable.is_dirty = hub_is_dirty;
    w->vtable.clear_dirty = hub_clear_dirty;
    w->vtable.destroy = hub_destroy;
    HubData *d = (HubData *)(w + 1);

    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    d->title = strdup(t && t->valuestring ? t->valuestring : "Configuration");
    d->cat_idx = 0; d->item_idx = 0;
    d->keys = NULL; d->vals = NULL; d->val_count = 0;
    d->mode = HUB_BROWSING;
    d->edit_text = NULL; d->edit_pass1 = NULL; d->edit_pass2 = NULL;
    d->edit_query = NULL; d->edit_filtered = NULL; d->edit_filtered_count = 0;
    d->edit_selected_set = NULL; d->dirty = true;

    cJSON *cats = cJSON_GetObjectItem(req->params, "categories");
    d->cat_count = 0; d->categories = NULL;
    if (cats && cats->type == cJSON_Array) {
        d->cat_count = cJSON_GetArraySize(cats);
        d->categories = calloc(d->cat_count, sizeof(HubCategory));
        int ci = 0;
        cJSON *cat_val;
        cJSON_ArrayForEach(cat_val, cats) {
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
                cJSON *ichf = cJSON_GetObjectItem(item_val, "choices_file");
                item->choice_count = 0;
                item->choices = NULL;

                if (ich && ich->type == cJSON_Array) {
                    item->choice_count = cJSON_GetArraySize(ich);
                    item->choices = item->choice_count > 0 ? malloc(item->choice_count * sizeof(char *)) : NULL;
                    for (int j = 0; j < item->choice_count; j++)
                        item->choices[j] = strdup(cJSON_GetArrayItem(ich, j)->valuestring);
                } else if (ichf && ichf->valuestring) {
                    char filepath[1024];
                    snprintf(filepath, sizeof(filepath), "/tmp/artix-installer/filly-data/%s", ichf->valuestring);
                    FILE *fp = fopen(filepath, "r");
                    if (fp) {
                        char line[4096];
                        int cap = 64;
                        item->choices = malloc(cap * sizeof(char *));
                        item->choice_count = 0;
                        while (fgets(line, sizeof(line), fp)) {
                            line[strcspn(line, "\n")] = 0;
                            if (strlen(line) == 0) continue;
                            if (item->choice_count >= cap) {
                                cap *= 2;
                                item->choices = realloc(item->choices, cap * sizeof(char *));
                            }
                            item->choices[item->choice_count++] = strdup(line);
                        }
                        fclose(fp);
                    }
                    if (item->choice_count == 0) {
                        item->choices = malloc(sizeof(char *));
                        item->choices[0] = strdup("(no data)");
                        item->choice_count = 1;
                    }
                }

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

    cJSON *acts = cJSON_GetObjectItem(req->params, "actions");
    d->action_count = 0; d->actions = NULL;
    if (acts && acts->type == cJSON_Array) {
        d->action_count = cJSON_GetArraySize(acts);
        d->actions = malloc(d->action_count * sizeof(char *));
        for (int i = 0; i < d->action_count; i++)
            d->actions[i] = strdup(cJSON_GetArrayItem(acts, i)->valuestring);
    }

    return w;
}