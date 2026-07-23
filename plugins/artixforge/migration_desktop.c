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
    char *current_de;
    char **des;
    int de_count;
    char **dm_choices;
    int dm_count;
    char **x_choices;
    int x_count;
    char **audio_choices;
    int audio_count;
    char **net_choices;
    int net_count;
    int target_idx, dm_idx, x_idx, audio_idx, net_idx;
    int field;
} MigDEData;

extern Arena *g_session_arena;

static void md_render(Widget *self, Rect area, RenderTree *out) {
    MigDEData *d = (MigDEData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = (int)(area.w * 0.55f), box_h = (int)(area.h * 0.55f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    const char *labels[] = {"Source DE", "Target DE", "Display Manager", "Display Stack", "Audio", "Network"};
    const char *values[] = {d->current_de, d->des[d->target_idx], d->dm_choices[d->dm_idx],
                            d->x_choices[d->x_idx], d->audio_choices[d->audio_idx], d->net_choices[d->net_idx]};
    RenderTree *children = arena_alloc(g_session_arena, 7 * sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = arena_strdup(g_session_arena, d->title);
    children[0].style_class = "text"; children[0].state = "title";

    for (int i = 0; i < 6; i++) {
        children[1 + i].type = RNODE_TEXT;
        children[1 + i].rect = rect_new(1, 1 + i, box_w - 2, 1);
        char buf[256];
        snprintf(buf, sizeof(buf), "%s %s: %s", d->field == i ? ">" : " ", labels[i], values[i]);
        children[1 + i].text.content = arena_strdup(g_session_arena, buf);
        children[1 + i].style_class = "text";
    }

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 7;
}

static EventResult md_handle(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    MigDEData *d = (MigDEData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_UP: d->field = d->field > 0 ? d->field - 1 : 0; d->base.dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->field < 5) d->field++; d->base.dirty = true; return event_result_handled();
        case KEY_LEFT:
            switch (d->field) {
                case 1: if (d->target_idx > 0) d->target_idx--; break;
                case 2: if (d->dm_idx > 0) d->dm_idx--; break;
                case 3: if (d->x_idx > 0) d->x_idx--; break;
                case 4: if (d->audio_idx > 0) d->audio_idx--; break;
                case 5: if (d->net_idx > 0) d->net_idx--; break;
            }
            d->base.dirty = true; return event_result_handled();
        case KEY_RIGHT:
            switch (d->field) {
                case 1: if (d->target_idx + 1 < d->de_count) d->target_idx++; break;
                case 2: if (d->dm_idx + 1 < d->dm_count) d->dm_idx++; break;
                case 3: if (d->x_idx + 1 < d->x_count) d->x_idx++; break;
                case 4: if (d->audio_idx + 1 < d->audio_count) d->audio_idx++; break;
                case 5: if (d->net_idx + 1 < d->net_count) d->net_idx++; break;
            }
            d->base.dirty = true; return event_result_handled();
        case KEY_ENTER:
            if (strcmp(d->current_de, d->des[d->target_idx]) == 0) return event_result_handled();
            cJSON *r = cJSON_CreateObject();
            cJSON_AddStringToObject(r, "source", d->current_de);
            cJSON_AddStringToObject(r, "target", d->des[d->target_idx]);
            cJSON_AddStringToObject(r, "dm", d->dm_choices[d->dm_idx]);
            cJSON_AddStringToObject(r, "x_stack", d->x_choices[d->x_idx]);
            cJSON_AddStringToObject(r, "audio", d->audio_choices[d->audio_idx]);
            cJSON_AddStringToObject(r, "network", d->net_choices[d->net_idx]);
            return event_result_response((WidgetResponse){ .result = r, .cancelled = false });
        default: return event_result_unhandled();
    }
}

static void md_destroy(Widget *self) {
    MigDEData *d = (MigDEData *)(self + 1);
    free(d->title); free(d->current_de);
    for (int i = 0; i < d->de_count; i++) free(d->des[i]);
    for (int i = 0; i < d->dm_count; i++) free(d->dm_choices[i]);
    for (int i = 0; i < d->x_count; i++) free(d->x_choices[i]);
    for (int i = 0; i < d->audio_count; i++) free(d->audio_choices[i]);
    for (int i = 0; i < d->net_count; i++) free(d->net_choices[i]);
    free(d->des); free(d->dm_choices); free(d->x_choices); free(d->audio_choices); free(d->net_choices);
}

Widget *migration_desktop_factory(const WidgetRequest *req) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(MigDEData));
    MigDEData data;
    memset(&data, 0, sizeof(data));
    cJSON *title_j = cJSON_GetObjectItem(req->params, "title");
    cJSON *de_j = cJSON_GetObjectItem(req->params, "current_de");
    data.title = strdup(title_j && title_j->valuestring ? title_j->valuestring : "Desktop Migration");
    data.current_de = strdup(de_j && de_j->valuestring ? de_j->valuestring : "none");
    const char *de_list[] = {"kde","sonicde","xfce","lxqt","lxde","hyprland","sway","niri","i3wm","dwm","vxwm","icewm","mango","cinnamon","budgie","moksha","cosmic","none"};
    data.de_count = 18; data.des = malloc(18 * sizeof(char *));
    for (int i = 0; i < 18; i++) data.des[i] = strdup(de_list[i]);
    const char *dm_list[] = {"current","sddm","lightdm","soniclogin","none"};
    data.dm_count = 5; data.dm_choices = malloc(5 * sizeof(char *));
    for (int i = 0; i < 5; i++) data.dm_choices[i] = strdup(dm_list[i]);
    const char *x_list[] = {"current","xlibre","xorg","wayland"};
    data.x_count = 4; data.x_choices = malloc(4 * sizeof(char *));
    for (int i = 0; i < 4; i++) data.x_choices[i] = strdup(x_list[i]);
    const char *a_list[] = {"current","pipewire","pulseaudio","none"};
    data.audio_count = 4; data.audio_choices = malloc(4 * sizeof(char *));
    for (int i = 0; i < 4; i++) data.audio_choices[i] = strdup(a_list[i]);
    const char *n_list[] = {"current","networkmanager","dhcpcd+iwd","connman","none"};
    data.net_count = 5; data.net_choices = malloc(5 * sizeof(char *));
    for (int i = 0; i < 5; i++) data.net_choices[i] = strdup(n_list[i]);
    data.target_idx = 0; data.dm_idx = 0; data.x_idx = 0; data.audio_idx = 0; data.net_idx = 0;
    data.field = 0;
    widget_base_init(w, &data, sizeof(MigDEData), md_render, md_handle, md_destroy);
    return w;
}