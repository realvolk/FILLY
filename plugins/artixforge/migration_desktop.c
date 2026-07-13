#include "../../filly-core/widget.h"
#include "../../filly-core/render.h"
#include "../../filly-protocol/protocol.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
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
    bool dirty;
} MigDEData;

static void md_render(Widget *self, Rect area, RenderTree *out) {
    MigDEData *d = (MigDEData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.55f), box_h = (int)(area.h * 0.55f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    const char *labels[] = {"Source DE", "Target DE", "Display Manager", "Display Stack", "Audio", "Network"};
    const char *values[] = {d->current_de, d->des[d->target_idx], d->dm_choices[d->dm_idx],
                            d->x_choices[d->x_idx], d->audio_choices[d->audio_idx], d->net_choices[d->net_idx]};
    RenderTree *children = calloc(7 + 1, sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = strdup(d->title);
    children[0].text.align = ALIGN_CENTER;
    children[0].text.style = textstyle_selected();

    for (int i = 0; i < 6; i++) {
        children[1 + i].type = RNODE_TEXT;
        children[1 + i].rect = rect_new(1, 1 + i, box_w - 2, 1);
        char buf[256];
        snprintf(buf, sizeof(buf), "%s %s: %s", d->field == i ? ">" : " ", labels[i], values[i]);
        children[1 + i].text.content = strdup(buf);
        children[1 + i].text.align = ALIGN_LEFT;
        children[1 + i].text.style = d->field == i ? textstyle_selected() : textstyle_normal();
    }

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 7;
}

static EventResult md_handle(Widget *self, Event *ev, Backend *backend) {
    MigDEData *d = (MigDEData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_UP: d->field = d->field > 0 ? d->field - 1 : 0; d->dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->field < 5) d->field++; d->dirty = true; return event_result_handled();
        case KEY_LEFT:
            switch (d->field) {
                case 1: if (d->target_idx > 0) d->target_idx--; break;
                case 2: if (d->dm_idx > 0) d->dm_idx--; break;
                case 3: if (d->x_idx > 0) d->x_idx--; break;
                case 4: if (d->audio_idx > 0) d->audio_idx--; break;
                case 5: if (d->net_idx > 0) d->net_idx--; break;
            }
            d->dirty = true; return event_result_handled();
        case KEY_RIGHT:
            switch (d->field) {
                case 1: if (d->target_idx + 1 < d->de_count) d->target_idx++; break;
                case 2: if (d->dm_idx + 1 < d->dm_count) d->dm_idx++; break;
                case 3: if (d->x_idx + 1 < d->x_count) d->x_idx++; break;
                case 4: if (d->audio_idx + 1 < d->audio_count) d->audio_idx++; break;
                case 5: if (d->net_idx + 1 < d->net_count) d->net_idx++; break;
            }
            d->dirty = true; return event_result_handled();
        case KEY_ENTER:
            if (strcmp(d->current_de, d->des[d->target_idx]) == 0) return event_result_handled();
            cJSON *r = cJSON_CreateObject();
            cJSON_AddStringToObject(r, "source", d->current_de);
            cJSON_AddStringToObject(r, "target", d->des[d->target_idx]);
            cJSON_AddStringToObject(r, "dm", d->dm_choices[d->dm_idx]);
            cJSON_AddStringToObject(r, "x_stack", d->x_choices[d->x_idx]);
            cJSON_AddStringToObject(r, "audio", d->audio_choices[d->audio_idx]);
            cJSON_AddStringToObject(r, "network", d->net_choices[d->net_idx]);
            return event_result_response((WidgetResponse){ .result = r, .cancelled = false, .error = NULL });
        default: return event_result_unhandled();
    }
}

static bool md_is_dirty(Widget *self) { return ((MigDEData *)(self + 1))->dirty; }
static void md_clear_dirty(Widget *self) { ((MigDEData *)(self + 1))->dirty = false; }
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
    w->vtable.render = md_render; w->vtable.handle_event = md_handle;
    w->vtable.is_dirty = md_is_dirty; w->vtable.clear_dirty = md_clear_dirty; w->vtable.destroy = md_destroy;
    MigDEData *d = (MigDEData *)(w + 1);
    d->title = strdup(cJSON_GetObjectItem(req->params, "title")->valuestring ?: "Desktop Migration");
    d->current_de = strdup(cJSON_GetObjectItem(req->params, "current_de")->valuestring ?: "none");
    const char *de_list[] = {"kde","sonicde","xfce","lxqt","lxde","hyprland","sway","niri","i3wm","dwm","vxwm","icewm","mango","cinnamon","budgie","moksha","cosmic","none"};
    d->de_count = 18; d->des = malloc(18 * sizeof(char *));
    for (int i = 0; i < 18; i++) d->des[i] = strdup(de_list[i]);
    const char *dm_list[] = {"current","sddm","lightdm","soniclogin","none"};
    d->dm_count = 5; d->dm_choices = malloc(5 * sizeof(char *));
    for (int i = 0; i < 5; i++) d->dm_choices[i] = strdup(dm_list[i]);
    const char *x_list[] = {"current","xlibre","xorg","wayland"};
    d->x_count = 4; d->x_choices = malloc(4 * sizeof(char *));
    for (int i = 0; i < 4; i++) d->x_choices[i] = strdup(x_list[i]);
    const char *a_list[] = {"current","pipewire","pulseaudio","none"};
    d->audio_count = 4; d->audio_choices = malloc(4 * sizeof(char *));
    for (int i = 0; i < 4; i++) d->audio_choices[i] = strdup(a_list[i]);
    const char *n_list[] = {"current","networkmanager","dhcpcd+iwd","connman","none"};
    d->net_count = 5; d->net_choices = malloc(5 * sizeof(char *));
    for (int i = 0; i < 5; i++) d->net_choices[i] = strdup(n_list[i]);
    d->target_idx = 0; d->dm_idx = 0; d->x_idx = 0; d->audio_idx = 0; d->net_idx = 0;
    d->field = 0; d->dirty = true;
    return w;
}