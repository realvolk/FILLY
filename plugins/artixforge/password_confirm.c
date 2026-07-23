#include "core/widget.h"
#include "core/render.h"
#include "core/widget_base.h"
#include "core/session.h"
#include "core/crypto.h"
#include "protocol/protocol.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    WidgetBase base;
    char *title;
    char *message;
    char *pass1;
    char *pass2;
    int field;
} PWConfirmData;

extern Arena *g_session_arena;

static void pw_render(Widget *self, Rect area, RenderTree *out) {
    PWConfirmData *d = (PWConfirmData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = (int)(area.w * 0.50f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.50f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    RenderTree *children = arena_alloc(g_session_arena, 5 * sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = arena_strdup(g_session_arena, d->title);
    children[0].style_class = "text"; children[0].state = "title";

    int y = 1;
    if (d->message && strlen(d->message)) {
        children[1].type = RNODE_TEXT;
        children[1].rect = rect_new(1, y, box_w - 2, 2);
        children[1].text.content = arena_strdup(g_session_arena, d->message);
        children[1].style_class = "text";
        y = 4;
    }

    const char *labels[] = {"Password:", "Confirm:"};
    const char *vals[] = {d->pass1, d->pass2};
    for (int i = 0; i < 2; i++) {
        int idx = (y == 1) ? 1 + i : 2 + i;
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, y, box_w - 2, 2);
        char buf[512], mask[128] = {0};
        for (int j = 0; j < (int)strlen(vals[i]) && j < 127; j++) mask[j] = '*';
        snprintf(buf, sizeof(buf), "%s %s\n%s", labels[i], mask, d->field == i ? ">" : " ");
        children[idx].text.content = arena_strdup(g_session_arena, buf);
        children[idx].style_class = "text";
        y += 2;
    }

    int footer_idx = (y == 1) ? 3 : 4;
    if (strlen(d->pass1) && strlen(d->pass2) && strcmp(d->pass1, d->pass2) != 0) {
        children[footer_idx].type = RNODE_TEXT;
        children[footer_idx].rect = rect_new(1, y, box_w - 2, 1);
        children[footer_idx].text.content = "Passwords do not match!";
        children[footer_idx].style_class = "text";
    } else {
        children[footer_idx].type = RNODE_TEXT;
        children[footer_idx].rect = rect_new(1, y, box_w - 2, 1);
        children[footer_idx].text.content = "Tab:next  Enter:submit  Esc:cancel";
        children[footer_idx].style_class = "text"; children[footer_idx].state = "muted";
    }

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 5;
}

static EventResult pw_handle(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    PWConfirmData *d = (PWConfirmData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_TAB:
            d->field = (d->field + 1) % 2; d->base.dirty = true; return event_result_handled();
        case KEY_ENTER:
            if (strlen(d->pass1) && strcmp(d->pass1, d->pass2) == 0) {
                char *hash = filly_hash_password(d->pass1);
                cJSON *result = cJSON_CreateString(hash);
                free(hash);
                return event_result_response((WidgetResponse){ .result = result, .cancelled = false });
            }
            return event_result_handled();
        case KEY_CHAR:
            if (d->field == 0) { int l = strlen(d->pass1); d->pass1 = realloc(d->pass1, l + 2); d->pass1[l] = ev->ch; d->pass1[l+1] = 0; }
            else { int l = strlen(d->pass2); d->pass2 = realloc(d->pass2, l + 2); d->pass2[l] = ev->ch; d->pass2[l+1] = 0; }
            d->base.dirty = true; return event_result_handled();
        case KEY_BACKSPACE:
            if (d->field == 0 && strlen(d->pass1)) d->pass1[strlen(d->pass1)-1] = 0;
            else if (d->field == 1 && strlen(d->pass2)) d->pass2[strlen(d->pass2)-1] = 0;
            d->base.dirty = true; return event_result_handled();
        default: return event_result_unhandled();
    }
}

static void pw_destroy(Widget *self) {
    PWConfirmData *d = (PWConfirmData *)(self + 1);
    free(d->title); free(d->message); free(d->pass1); free(d->pass2);
}

Widget *password_confirm_factory(const WidgetRequest *req) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(PWConfirmData));
    PWConfirmData data;
    memset(&data, 0, sizeof(data));
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *m = cJSON_GetObjectItem(req->params, "message");
    data.title = strdup(t && t->valuestring ? t->valuestring : "Password");
    data.message = m && m->valuestring ? strdup(m->valuestring) : strdup("");
    data.pass1 = strdup(""); data.pass2 = strdup("");
    data.field = 0;
    widget_base_init(w, &data, sizeof(PWConfirmData), pw_render, pw_handle, pw_destroy);
    return w;
}