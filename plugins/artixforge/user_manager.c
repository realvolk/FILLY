#include "../../filly-core/widget.h"
#include "../../filly-core/render.h"
#include "../../filly-protocol/protocol.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char *hash_password(const char *p) {
    if (!p || !*p) return strdup("");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "echo '%s' | openssl passwd -6 --stdin 2>/dev/null", p);
    FILE *fp = popen(cmd, "r");
    if (!fp) return strdup("");
    char buf[256] = {0};
    fread(buf, 1, sizeof(buf), fp);
    pclose(fp);
    char *nl = strchr(buf, '\n'); if (nl) *nl = 0;
    return strdup(buf);
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
    UserEntry *users;
    int user_count;
    int mode; // 0=browse, 1=add, 2=confirm_delete
    int selected;
    int del_idx;
    AddUserState add_state;
    bool dirty;
} UMData;

static const char *shells[] = {"/bin/bash", "/bin/zsh", "/usr/bin/fish"};
static const char *all_groups[] = {"wheel", "audio", "video", "storage", "lp", "network", "optical", "scanner", "users"};
static int all_group_count = 9;

static void um_render(Widget *self, Rect area, RenderTree *out) {
    UMData *d = (UMData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.70f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.80f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    RenderTree *children = calloc(4 + 1, sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = strdup(d->mode == 1 ? "Add / Edit User" : d->mode == 2 ? "Delete User" : "Manage Users");
    children[0].text.align = ALIGN_CENTER;
    children[0].text.style = textstyle_selected();

    if (d->mode == 2) {
        children[1].type = RNODE_TEXT;
        children[1].rect = rect_new(1, 1, box_w - 2, 1);
        char buf[256];
        snprintf(buf, sizeof(buf), "Delete user '%s'?", d->users[d->del_idx].name);
        children[1].text.content = strdup(buf);
        children[1].text.align = ALIGN_LEFT;
        children[1].text.style = textstyle_normal();
        children[2].type = RNODE_TEXT;
        children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
        children[2].text.content = strdup("Y:yes  N:no");
        children[2].text.align = ALIGN_CENTER;
        children[2].text.style = textstyle_muted();
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
            children[1].text.content = strdup("No users configured. Press A to add one.");
            children[1].text.align = ALIGN_LEFT;
            children[1].text.style = textstyle_muted();
        } else {
            children[1].type = RNODE_LIST;
            children[1].rect = rect_new(1, 1, box_w - 2, box_h - 4);
            children[1].list.item_count = d->user_count;
            children[1].list.items = malloc(d->user_count * sizeof(ListItem));
            for (int i = 0; i < d->user_count; i++) {
                char label[256];
                snprintf(label, sizeof(label), "%s%s (%s) [%s]", i == d->selected ? ">" : "  ",
                    d->users[i].name, d->users[i].shell, d->users[i].sudo ? "sudo" : "nosudo");
                children[1].list.items[i] = listitem_new(label);
            }
            children[1].list.selected = d->selected;
            children[1].list.highlight = textstyle_selected();
        }
        children[2].type = RNODE_TEXT;
        children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
        children[2].text.content = strdup("A:add  D:delete  Enter:edit  Esc:done");
        children[2].text.align = ALIGN_CENTER;
        children[2].text.style = textstyle_muted();
    } else {
        AddUserState *s = &d->add_state;
        const char *fields[] = {
            s->name, s->pass, s->confirm_pass,
            shells[s->shell_idx],
            s->groups ? "groups" : "",
            s->sudo ? "yes" : "no"
        };
        const char *labels[] = {"Username:", "Password:", "Confirm:", "Shell:", "Groups:", "Sudo:"};
        children[1].type = RNODE_TEXT;
        children[1].rect = rect_new(1, 1, box_w - 2, box_h - 3);
        char *text = strdup("");
        for (int i = 0; i < 6; i++) {
            char line[512];
            char mask[128] = {0};
            const char *val = fields[i];
            if ((i == 1 || i == 2) && strlen(val)) {
                for (int j = 0; j < (int)strlen(val) && j < 127; j++) mask[j] = '*';
                val = mask;
            }
            if (i == 4) {
                char gbuf[256] = {0};
                for (int j = 0; j < s->group_count && s->groups; j++) {
                    if (j > 0) strcat(gbuf, ",");
                    strcat(gbuf, s->groups[j]);
                }
                val = gbuf;
            }
            snprintf(line, sizeof(line), "%s %s %s\n", i == s->field ? ">" : " ", labels[i], val && strlen(val) ? val : "(required)");
            char *t = malloc(strlen(text) + strlen(line) + 1);
            strcpy(t, text); strcat(t, line);
            free(text); text = t;
        }
        children[1].text.content = text;
        children[1].text.align = ALIGN_LEFT;
        children[1].text.style = textstyle_normal();

        if (strlen(s->pass) && strlen(s->confirm_pass) && strcmp(s->pass, s->confirm_pass)) {
            children[2].type = RNODE_TEXT;
            children[2].rect = rect_new(1, box_h - 3, box_w - 2, 1);
            children[2].text.content = strdup("Passwords do not match!");
            children[2].text.align = ALIGN_LEFT;
            children[2].text.style = (TextStyle){ .fg = strdup("1"), .bold = true };
        }
        children[3].type = RNODE_TEXT;
        children[3].rect = rect_new(1, box_h - 2, box_w - 2, 1);
        children[3].text.content = strdup("Tab:next  Enter:save  Esc:cancel");
        children[3].text.align = ALIGN_CENTER;
        children[3].text.style = textstyle_muted();
    }

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 4;
}

static EventResult um_handle(Widget *self, Event *ev, Backend *backend) {
    UMData *d = (UMData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();

    if (d->mode == 2) {
        if (ev->code == KEY_CHAR && (ev->ch == 'y' || ev->ch == 'Y')) {
            memmove(&d->users[d->del_idx], &d->users[d->del_idx + 1], (d->user_count - d->del_idx - 1) * sizeof(UserEntry));
            d->user_count--;
            if (d->selected >= d->user_count && d->selected > 0) d->selected--;
            d->mode = 0; d->dirty = true;
            return event_result_handled();
        }
        d->mode = 0; d->dirty = true; return event_result_handled();
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
                return event_result_response((WidgetResponse){ .result = arr, .cancelled = false, .error = NULL });
            }
            case KEY_UP: if (d->selected > 0) d->selected--; d->dirty = true; return event_result_handled();
            case KEY_DOWN: if (d->selected + 1 < d->user_count) d->selected++; d->dirty = true; return event_result_handled();
            case KEY_CHAR:
                if (ev->ch == 'a' || ev->ch == 'A') {
                    d->add_state.name = strdup(""); d->add_state.pass = strdup("");
                    d->add_state.confirm_pass = strdup(""); d->add_state.shell_idx = 0;
                    d->add_state.groups = malloc(4 * sizeof(char *));
                    d->add_state.groups[0] = strdup("wheel"); d->add_state.groups[1] = strdup("audio");
                    d->add_state.groups[2] = strdup("video"); d->add_state.groups[3] = strdup("storage");
                    d->add_state.group_count = 4;
                    d->add_state.sudo = true; d->add_state.field = 0;
                    d->mode = 1; d->dirty = true;
                    return event_result_handled();
                }
                if ((ev->ch == 'd' || ev->ch == 'D') && d->selected < d->user_count) {
                    d->del_idx = d->selected; d->mode = 2; d->dirty = true;
                    return event_result_handled();
                }
                return event_result_unhandled();
            case KEY_ENTER:
                if (d->selected < d->user_count) {
                    UserEntry *u = &d->users[d->selected];
                    d->add_state.name = strdup(u->name); d->add_state.pass = strdup("");
                    d->add_state.confirm_pass = strdup(""); d->add_state.shell_idx = 0;
                    for (int i = 0; i < 3; i++) if (strcmp(shells[i], u->shell) == 0) d->add_state.shell_idx = i;
                    d->add_state.groups = malloc(u->group_count * sizeof(char *));
                    d->add_state.group_count = u->group_count;
                    for (int i = 0; i < u->group_count; i++) d->add_state.groups[i] = strdup(u->groups[i]);
                    d->add_state.sudo = u->sudo; d->add_state.field = 0;
                    d->mode = 1; d->dirty = true;
                }
                return event_result_handled();
            default: return event_result_unhandled();
        }
    }

    AddUserState *s = &d->add_state;
    switch (ev->code) {
        case KEY_ESC: d->mode = 0; d->dirty = true; return event_result_handled();
        case KEY_TAB: case KEY_DOWN: s->field = (s->field + 1) % 6; d->dirty = true; return event_result_handled();
        case KEY_UP: s->field = (s->field + 5) % 6; d->dirty = true; return event_result_handled();
        case KEY_ENTER:
            if (strlen(s->name) && strlen(s->pass) && strcmp(s->pass, s->confirm_pass) == 0) {
                UserEntry entry = {
                    .name = strdup(s->name),
                    .pass = hash_password(s->pass),
                    .shell = strdup(shells[s->shell_idx]),
                    .groups = malloc(s->group_count * sizeof(char *)),
                    .group_count = s->group_count,
                    .sudo = s->sudo
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
                d->mode = 0; d->dirty = true;
            }
            return event_result_handled();
        case KEY_LEFT:
            if (s->field == 3) s->shell_idx = (s->shell_idx + 2) % 3;
            else if (s->field == 5) s->sudo = !s->sudo;
            d->dirty = true; return event_result_handled();
        case KEY_RIGHT:
            if (s->field == 3) s->shell_idx = (s->shell_idx + 1) % 3;
            else if (s->field == 5) s->sudo = !s->sudo;
            d->dirty = true; return event_result_handled();
        case KEY_CHAR:
            if (s->field == 0) { int l = strlen(s->name); s->name = realloc(s->name, l+2); s->name[l]=ev->ch; s->name[l+1]=0; }
            else if (s->field == 1) { int l = strlen(s->pass); s->pass = realloc(s->pass, l+2); s->pass[l]=ev->ch; s->pass[l+1]=0; }
            else if (s->field == 2) { int l = strlen(s->confirm_pass); s->confirm_pass = realloc(s->confirm_pass, l+2); s->confirm_pass[l]=ev->ch; s->confirm_pass[l+1]=0; }
            d->dirty = true; return event_result_handled();
        case KEY_BACKSPACE:
            if (s->field == 0 && strlen(s->name)) s->name[strlen(s->name)-1]=0;
            else if (s->field == 1 && strlen(s->pass)) s->pass[strlen(s->pass)-1]=0;
            else if (s->field == 2 && strlen(s->confirm_pass)) s->confirm_pass[strlen(s->confirm_pass)-1]=0;
            d->dirty = true; return event_result_handled();
        default: return event_result_unhandled();
    }
}

static bool um_is_dirty(Widget *self) { return ((UMData *)(self + 1))->dirty; }
static void um_clear_dirty(Widget *self) { ((UMData *)(self + 1))->dirty = false; }
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
    Widget *w = calloc(1, sizeof(Widget) + sizeof(UMData));
    w->vtable.render = um_render;
    w->vtable.handle_event = um_handle;
    w->vtable.is_dirty = um_is_dirty;
    w->vtable.clear_dirty = um_clear_dirty;
    w->vtable.destroy = um_destroy;
    UMData *d = (UMData *)(w + 1);
    d->users = NULL; d->user_count = 0;
    d->mode = 0; d->selected = 0; d->dirty = true;
    return w;
}