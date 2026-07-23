#include "core/widget.h"
#include "core/render.h"
#include "core/widget_base.h"
#include "core/session.h"
#include "protocol/protocol.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "core/crypto.h"

static char *hash_password(const char *p) {
    return filly_hash_password(p);
}

typedef struct {
    char *name;
    char *pass;
    char *shell;
    char **groups;
    int group_count;
    bool sudo;
} UserEntry;

typedef struct {
    char *name;
    char *pass;
    char *confirm_pass;
    int shell_idx;
    char **groups;
    int group_count;
    bool sudo;
    int field;
} AddUserState;

typedef struct {
    WidgetBase base;
    UserEntry *users;
    int user_count;
    int mode;
    int selected;
    int del_idx;
    AddUserState add_state;
} UMData;

static const char *shells[] = {"/bin/bash", "/bin/zsh", "/usr/bin/fish"};
static const char *all_groups[] = {"wheel", "audio", "video", "storage", "lp", "network", "optical", "scanner", "users"};
static int all_group_count = 9;

extern Arena *g_session_arena;

static void um_render(Widget *self, Rect area, RenderTree *out) {
    UMData *d = (UMData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = (int)(area.w * 0.70f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.80f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    RenderTree *children = arena_alloc(g_session_arena, 4 * sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = arena_strdup(g_session_arena,
        d->mode == 1 ? "Add / Edit User" : d->mode == 2 ? "Delete User" : "Manage Users");
        d->base.accepts_text_input = true;
    children[0].style_class = "text"; children[0].state = "title";

    if (d->mode == 2) {
        children[1].type = RNODE_TEXT;
        children[1].rect = rect_new(1, 1, box_w - 2, 1);
        char buf[256];
        snprintf(buf, sizeof(buf), "Delete user '%s'?", d->users[d->del_idx].name);
        children[1].text.content = arena_strdup(g_session_arena, buf);
        children[1].style_class = "text";
        children[2].type = RNODE_TEXT;
        children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
        children[2].text.content = "Y:yes  N:no";
        children[2].style_class = "text"; children[2].state = "muted";
        out->type = RNODE_CONTAINER;
        out->rect = rect_new(box_x, box_y, box_w, box_h);
        out->container.border = BORDER_SINGLE;
        out->container.padding = edgeinsets_zero();
        out->container.children = children;
        out->container.child_count = 3;
        return;
    }

    if (d->mode == 0) {
        if (d->user_count == 0) {
            children[1].type = RNODE_TEXT;
            children[1].rect = rect_new(1, 2, box_w - 2, 1);
            children[1].text.content = "No users configured. Press A to add one.";
            children[1].style_class = "text"; children[1].state = "muted";
        } else {
            children[1].type = RNODE_LIST;
            children[1].rect = rect_new(1, 1, box_w - 2, box_h - 4);
            children[1].list.item_count = d->user_count;
            children[1].list.selected = d->selected;
            children[1].list.items = arena_alloc(g_session_arena, d->user_count * sizeof(ListItem));
            for (int i = 0; i < d->user_count; i++) {
                char label[256];
                snprintf(label, sizeof(label), "%s%s (%s) [%s]", i == d->selected ? ">" : "  ",
                    d->users[i].name, d->users[i].shell, d->users[i].sudo ? "sudo" : "nosudo");
                children[1].list.items[i].label = arena_strdup(g_session_arena, label);
            }
            children[1].style_class = "list";
        }
        children[2].type = RNODE_TEXT;
        children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
        children[2].text.content = "A:add  D:delete  Enter:edit  Esc:done";
        children[2].style_class = "text"; children[2].state = "muted";
    } else {
        AddUserState *s = &d->add_state;
        children[1].type = RNODE_TEXT;
        children[1].rect = rect_new(1, 1, box_w - 2, box_h - 3);
        char *text = strdup("");
        const char *labels[] = {"Username:", "Password:", "Confirm:", "Shell:", "Groups:", "Sudo:"};
        for (int i = 0; i < 6; i++) {
            char line[512]; char mask[128] = {0};
            const char *val = i == 0 ? s->name : i == 1 ? s->pass : i == 2 ? s->confirm_pass :
                              i == 3 ? shells[s->shell_idx] : i == 5 ? (s->sudo ? "yes" : "no") : "";
            if ((i == 1 || i == 2) && strlen(val))
                for (int j = 0; j < (int)strlen(val) && j < 127; j++) mask[j] = '*';
            if (i == 4) {
                char gbuf[256] = {0};
                for (int j = 0; j < s->group_count && s->groups; j++) {
                    if (j > 0) strcat(gbuf, ",");
                    strcat(gbuf, s->groups[j]);
                }
                val = gbuf;
            }
            if ((i == 1 || i == 2) && strlen(val)) val = mask;
            snprintf(line, sizeof(line), "%s %s %s\n", i == s->field ? ">" : " ", labels[i], val && strlen(val) ? val : "(required)");
            char *t = malloc(strlen(text) + strlen(line) + 1);
            strcpy(t, text); strcat(t, line);
            free(text); text = t;
        }
        children[1].text.content = arena_strdup(g_session_arena, text);
        free(text);
        children[1].style_class = "text";

        if (strlen(s->pass) && strlen(s->confirm_pass) && strcmp(s->pass, s->confirm_pass)) {
            children[2].type = RNODE_TEXT;
            children[2].rect = rect_new(1, box_h - 3, box_w - 2, 1);
            children[2].text.content = "Passwords do not match!";
            children[2].style_class = "text";
        }
        children[3].type = RNODE_TEXT;
        children[3].rect = rect_new(1, box_h - 2, box_w - 2, 1);
        children[3].text.content = "Tab:next  Enter:save  Esc:cancel";
        children[3].style_class = "text"; children[3].state = "muted";
    }

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 4;
}

static EventResult um_handle(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    UMData *d = (UMData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();

    if (d->mode == 2) {
        if (ev->code == KEY_CHAR && (ev->ch == 'y' || ev->ch == 'Y')) {
            memmove(&d->users[d->del_idx], &d->users[d->del_idx + 1], (d->user_count - d->del_idx - 1) * sizeof(UserEntry));
            d->user_count--;
            if (d->selected >= d->user_count && d->selected > 0) d->selected--;
            d->mode = 0;
            d->base.dirty = true;
            d->base.accepts_text_input = false;
            return event_result_handled();
        }
        d->mode = 0;
        d->base.dirty = true;
        d->base.accepts_text_input = false;
        return event_result_handled();
    }

    if (d->mode == 0) {
        switch (ev->code) {
            case KEY_ESC: {
                cJSON *arr = cJSON_CreateArray();
                for (int i = 0; i < d->user_count; i++) {
                    cJSON *u = cJSON_CreateObject();
                    cJSON_AddStringToObject(u, "name", d->users[i].name);
                    cJSON_AddStringToObject(u, "pass", d->users[i].pass);
                    cJSON_AddStringToObject(u, "shell", d->users[i].shell);
                    char gbuf[256] = {0};
                    for (int j = 0; j < d->users[i].group_count; j++) {
                        if (j > 0) strcat(gbuf, ",");
                        strcat(gbuf, d->users[i].groups[j]);
                    }
                    cJSON_AddStringToObject(u, "groups", gbuf);
                    cJSON_AddBoolToObject(u, "sudo", d->users[i].sudo);
                    cJSON_AddItemToArray(arr, u);
                }
                return event_result_response((WidgetResponse){ .result = arr, .cancelled = false });
            }
            case KEY_UP: if (d->selected > 0) d->selected--; d->base.dirty = true; return event_result_handled();
            case KEY_DOWN: if (d->selected + 1 < d->user_count) d->selected++; d->base.dirty = true; return event_result_handled();
            case KEY_CHAR:
                if (ev->ch == 'a' || ev->ch == 'A') {
                    AddUserState *s = &d->add_state;
                    s->name = strdup(""); s->pass = strdup(""); s->confirm_pass = strdup("");
                    s->shell_idx = 0; s->group_count = 4;
                    s->groups = malloc(4 * sizeof(char *));
                    s->groups[0] = strdup("wheel"); s->groups[1] = strdup("audio");
                    s->groups[2] = strdup("video"); s->groups[3] = strdup("storage");
                    s->sudo = true; s->field = 0;
                    d->mode = 1;
                    d->base.dirty = true;
                    d->base.accepts_text_input = true;
                    return event_result_handled();
                }
                if ((ev->ch == 'd' || ev->ch == 'D') && d->selected < d->user_count) {
                    d->del_idx = d->selected; d->mode = 2; d->base.dirty = true;
                    return event_result_handled();
                }
                return event_result_unhandled();
            case KEY_ENTER:
                if (d->selected < d->user_count) {
                    UserEntry *u = &d->users[d->selected];
                    AddUserState *s = &d->add_state;
                    s->name = strdup(u->name); s->pass = strdup(""); s->confirm_pass = strdup("");
                    s->shell_idx = 0;
                    for (int i = 0; i < 3; i++) if (strcmp(shells[i], u->shell) == 0) s->shell_idx = i;
                    s->groups = malloc(u->group_count * sizeof(char *));
                    s->group_count = u->group_count;
                    for (int i = 0; i < u->group_count; i++) s->groups[i] = strdup(u->groups[i]);
                    s->sudo = u->sudo; s->field = 0;
                    d->mode = 1;
                    d->base.dirty = true;
                    d->base.accepts_text_input = true;
                }
                return event_result_handled();
            default: return event_result_unhandled();
        }
    }

    AddUserState *s = &d->add_state;
    switch (ev->code) {
        case KEY_ESC:
            d->mode = 0;
            d->base.dirty = true;
            d->base.accepts_text_input = false;
            return event_result_handled();
        case KEY_TAB: case KEY_DOWN: s->field = (s->field + 1) % 6; d->base.dirty = true; return event_result_handled();
        case KEY_UP: s->field = (s->field + 5) % 6; d->base.dirty = true; return event_result_handled();
        case KEY_ENTER:
            if (strlen(s->name) && strlen(s->pass) && strcmp(s->pass, s->confirm_pass) == 0) {
                UserEntry entry = {
                    .name = strdup(s->name), .pass = hash_password(s->pass),
                    .shell = strdup(shells[s->shell_idx]),
                    .groups = malloc(s->group_count * sizeof(char *)),
                    .group_count = s->group_count, .sudo = s->sudo
                };
                for (int i = 0; i < s->group_count; i++) entry.groups[i] = strdup(s->groups[i]);
                if (d->selected < d->user_count) {
                    free(d->users[d->selected].name); free(d->users[d->selected].pass);
                    free(d->users[d->selected].shell);
                    for (int i = 0; i < d->users[d->selected].group_count; i++) free(d->users[d->selected].groups[i]);
                    free(d->users[d->selected].groups);
                    d->users[d->selected] = entry;
                } else {
                    d->users = realloc(d->users, (d->user_count + 1) * sizeof(UserEntry));
                    d->users[d->user_count++] = entry;
                }
                d->mode = 0;
                d->base.dirty = true;
                d->base.accepts_text_input = false;
            }
            return event_result_handled();
        case KEY_LEFT:
            if (s->field == 3) s->shell_idx = (s->shell_idx + 2) % 3;
            else if (s->field == 5) s->sudo = !s->sudo;
            d->base.dirty = true; return event_result_handled();
        case KEY_RIGHT:
            if (s->field == 3) s->shell_idx = (s->shell_idx + 1) % 3;
            else if (s->field == 5) s->sudo = !s->sudo;
            d->base.dirty = true; return event_result_handled();
        case KEY_CHAR:
            if (s->field == 0) { int l = strlen(s->name); s->name = realloc(s->name, l+2); s->name[l]=ev->ch; s->name[l+1]=0; }
            else if (s->field == 1) { int l = strlen(s->pass); s->pass = realloc(s->pass, l+2); s->pass[l]=ev->ch; s->pass[l+1]=0; }
            else if (s->field == 2) { int l = strlen(s->confirm_pass); s->confirm_pass = realloc(s->confirm_pass, l+2); s->confirm_pass[l]=ev->ch; s->confirm_pass[l+1]=0; }
            d->base.dirty = true; return event_result_handled();
        case KEY_BACKSPACE:
            if (s->field == 0 && strlen(s->name)) s->name[strlen(s->name)-1]=0;
            else if (s->field == 1 && strlen(s->pass)) s->pass[strlen(s->pass)-1]=0;
            else if (s->field == 2 && strlen(s->confirm_pass)) s->confirm_pass[strlen(s->confirm_pass)-1]=0;
            d->base.dirty = true; return event_result_handled();
        default: return event_result_unhandled();
    }
}

static void um_destroy(Widget *self) {
    UMData *d = (UMData *)(self + 1);
    for (int i = 0; i < d->user_count; i++) {
        free(d->users[i].name); free(d->users[i].pass); free(d->users[i].shell);
        for (int j = 0; j < d->users[i].group_count; j++) free(d->users[i].groups[j]);
        free(d->users[i].groups);
    }
    free(d->users);
    free(d->add_state.name); free(d->add_state.pass); free(d->add_state.confirm_pass);
    for (int i = 0; i < d->add_state.group_count; i++) free(d->add_state.groups[i]);
    free(d->add_state.groups);
}

Widget *user_manager_factory(const WidgetRequest *req) {
    (void)req; (void)all_groups; (void)all_group_count;
    Widget *w = calloc(1, sizeof(Widget) + sizeof(UMData));
    UMData data;
    memset(&data, 0, sizeof(data));
    data.users = NULL; data.user_count = 0;
    data.mode = 0; data.selected = 0;
    widget_base_init(w, &data, sizeof(UMData), um_render, um_handle, um_destroy);
    return w;
}