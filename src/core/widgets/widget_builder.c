/* ===== src/core/widgets/widget_builder.c ===== */
#include "widget_builder.h"
#include "core/widget_base.h"
#include "core/session.h"
#include "core/undo.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

typedef struct {
    char *widget_type;
    char *title;
    cJSON *params;
    int x, y, w, h;
    Widget *live_widget;
} BuilderItem;

typedef enum { BMODE_PALETTE, BMODE_CANVAS, BMODE_PROPERTIES, BMODE_LIVE_PREVIEW } BuilderMode;

typedef struct {
    WidgetBase base;
    BuilderItem *items;
    int item_count;
    int selected_item;
    BuilderMode mode;
    int palette_idx;
    char **palette_types;
    int palette_count;
    int canvas_ox, canvas_oy;
    float scale;
    int drag_item;
    int drag_start_x, drag_start_y;
    int drag_orig_x, drag_orig_y;
    bool dragging;
    int resize_handle;
    int prop_edit_field;
    char prop_edit_buf[256];
    int prop_edit_len;
    bool prop_editing;
    UndoStack *undo;
} WidgetBuilderData;

extern Arena *g_session_arena;

static const char *builtin_palette[] = {
    "menu", "yesno", "input", "password", "msg", "filter", "checklist",
    "multiselect", "toggle", "range_slider", "calendar", "form", "progress",
    "table", "tree", "gauge", "separator", "badge", "spinner", "rich_text",
    "tooltip", "notification", "context_menu", "file_picker", "text_editor",
    "summary", "disk", "color_picker", "radio_group", "tabs", "split_panes",
    "hub", "terminal_emulator"
};

static Widget *create_live_widget(const char *type, const char *title, cJSON *params) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "widget", type);
    cJSON *p = cJSON_Duplicate(params, 1);
    if (!cJSON_GetObjectItem(p, "title"))
        cJSON_AddStringToObject(p, "title", title);
    cJSON_AddItemToObject(root, "params", p);
    char *json = cJSON_PrintUnformatted(root);
    WidgetRequest *req = widget_request_parse(json);
    free(json);
    cJSON_Delete(root);
    if (!req) return NULL;
    Widget *w = widget_registry_create(req);
    widget_request_free(req);
    return w;
}

static void rebuild_live_widget(BuilderItem *item) {
    if (item->live_widget) widget_destroy(item->live_widget);
    item->live_widget = create_live_widget(item->widget_type, item->title, item->params);
}

static void render_scaled_tree(RenderTree *src, RenderTree *dst, float scale, int ox, int oy, Arena *arena) {
    if (!src || !dst) return;
    *dst = *src;
    dst->rect.x = (int)(src->rect.x * scale) + ox;
    dst->rect.y = (int)(src->rect.y * scale) + oy;
    dst->rect.w = (int)(src->rect.w * scale);
    dst->rect.h = (int)(src->rect.h * scale);
    dst->resolved_style.font_size = (int)(src->resolved_style.font_size * scale);
    if (dst->resolved_style.font_size < 6) dst->resolved_style.font_size = 6;
    dst->resolved_style.border_width = (int)(src->resolved_style.border_width * scale);
    dst->resolved_style.padding[0] = (int)(src->resolved_style.padding[0] * scale);
    dst->resolved_style.padding[1] = (int)(src->resolved_style.padding[1] * scale);
    dst->resolved_style.padding[2] = (int)(src->resolved_style.padding[2] * scale);
    dst->resolved_style.padding[3] = (int)(src->resolved_style.padding[3] * scale);
    if (dst->type == RNODE_CONTAINER && dst->u.container.children && dst->u.container.child_count > 0) {
        dst->u.container.children = arena_alloc(arena, dst->u.container.child_count * sizeof(RenderTree));
        for (int i = 0; i < dst->u.container.child_count; i++)
            render_scaled_tree(&src->u.container.children[i], &dst->u.container.children[i], scale,
                              ox + (int)(src->rect.x * scale), oy + (int)(src->rect.y * scale), arena);
    }
}

static void builder_push_undo(WidgetBuilderData *d, const char *desc) {
    WidgetAction *a = calloc(1, sizeof(WidgetAction));
    a->description = strdup(desc);
    undo_stack_push(d->undo, a);
}

static void builder_render(Widget *self, RenderTree *out) {
    WidgetBuilderData *d = (WidgetBuilderData *)(self + 1);
    WidgetBase *base = (WidgetBase *)(self + 1);
    Rect area = base->render_area;
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = area.w - 2;
    int box_h = area.h - 2;
    if (box_w < 40) box_w = 40;
    if (box_h < 15) box_h = 15;

    if (d->mode == BMODE_LIVE_PREVIEW) {
        if (d->selected_item >= 0 && d->selected_item < d->item_count && d->items[d->selected_item].live_widget) {
            Widget *lw = d->items[d->selected_item].live_widget;
            RenderTree tree;
            memset(&tree, 0, sizeof(tree));
            lw->vtable.render(lw, &tree);
            if (g_active_theme) resolve_node_styles(&tree, g_active_theme);
            *out = tree;
        } else {
            out->type = RNODE_TEXT;
            out->rect = rect_new(0, 0, box_w, box_h);
            out->u.text.content = "No widget selected for preview";
        }
        return;
    }

    int left_w = box_w * 20 / 100;
    int center_x = left_w + 2;
    int center_w = box_w * 55 / 100;
    int right_x = center_x + center_w + 2;
    int right_w = box_w - right_x - 1;

    RenderTree *children = arena_alloc(g_session_arena, 6 * sizeof(RenderTree));

    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w, 1);
    children[0].u.text.content = arena_strdup(g_session_arena, "FILLY Widget Builder");
    children[0].style_class = "text";
    children[0].state = "title";

    children[1].type = RNODE_LIST;
    children[1].rect = rect_new(1, 1, left_w, box_h - 3);
    children[1].u.list.item_count = d->item_count;
    children[1].u.list.selected = d->selected_item;
    children[1].u.list.items = arena_alloc(g_session_arena, d->item_count * sizeof(ListItem));
    for (int i = 0; i < d->item_count; i++)
        children[1].u.list.items[i].label = arena_strdup(g_session_arena, d->items[i].widget_type);
    children[1].style_class = "list";

    children[2].type = RNODE_CONTAINER;
    children[2].rect = rect_new(center_x, 1, center_w, box_h - 3);
    children[2].u.container.border = BORDER_SINGLE;
    children[2].u.container.padding = edgeinsets_zero();
    children[2].style_class = "canvas";

    int canvas_child_count = d->item_count;
    RenderTree *canvas_children = arena_alloc(g_session_arena, canvas_child_count * sizeof(RenderTree));
    for (int i = 0; i < d->item_count; i++) {
        BuilderItem *item = &d->items[i];
        canvas_children[i].type = RNODE_CONTAINER;
        canvas_children[i].rect = rect_new((int)(item->x * d->scale) + d->canvas_ox,
                                            (int)(item->y * d->scale) + d->canvas_oy,
                                            (int)(item->w * d->scale),
                                            (int)(item->h * d->scale));
        canvas_children[i].u.container.border = BORDER_SINGLE;
        canvas_children[i].style_class = i == d->selected_item ? "selected" : "normal";
        if (item->live_widget) {
            RenderTree preview;
            memset(&preview, 0, sizeof(preview));
            item->live_widget->vtable.render(item->live_widget, &preview);
            RenderTree *scaled = arena_alloc(g_session_arena, sizeof(RenderTree));
            render_scaled_tree(&preview, scaled, d->scale,
                              (int)(item->x * d->scale) + d->canvas_ox,
                              (int)(item->y * d->scale) + d->canvas_oy,
                              g_session_arena);
            canvas_children[i] = *scaled;
        }
    }
    children[2].u.container.children = canvas_children;
    children[2].u.container.child_count = canvas_child_count;

    children[3].type = RNODE_LIST;
    children[3].rect = rect_new(right_x, 1, right_w, box_h - 3);
    children[3].u.list.item_count = d->palette_count;
    children[3].u.list.selected = d->palette_idx;
    children[3].u.list.items = arena_alloc(g_session_arena, d->palette_count * sizeof(ListItem));
    for (int i = 0; i < d->palette_count; i++)
        children[3].u.list.items[i].label = arena_strdup(g_session_arena, d->palette_types[i]);
    children[3].style_class = "list";

    children[4].type = RNODE_TEXT;
    children[4].rect = rect_new(1, box_h - 2, box_w, 2);
    char props[4096] = {0};
    if (d->selected_item >= 0 && d->selected_item < d->item_count) {
        BuilderItem *item = &d->items[d->selected_item];
        if (d->prop_editing) {
            snprintf(props, sizeof(props), "Editing: %s > %s_", item->widget_type, d->prop_edit_buf);
        } else {
            char *pj = cJSON_PrintUnformatted(item->params);
            snprintf(props, sizeof(props), "%s | %s | %d,%d %dx%d | %s",
                item->widget_type, item->title, item->x, item->y, item->w, item->h, pj);
            free(pj);
        }
    } else {
        strcpy(props, "Click palette to add, drag canvas items, F5 preview, S save, Ctrl+Z undo");
    }
    children[4].u.text.content = arena_strdup(g_session_arena, props);
    children[4].style_class = "text";

    children[5].type = RNODE_TEXT;
    children[5].rect = rect_new(1, box_h, box_w, 1);
    children[5].u.text.content = "F1:tree F2:canvas F3:palette F4:props F5:preview S:save Del:remove Esc:quit";
    children[5].style_class = "text";
    children[5].state = "muted";

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(0, 0, area.w, area.h);
    out->u.container.border = BORDER_SINGLE;
    out->u.container.padding = edgeinsets_zero();
    out->u.container.children = children;
    out->u.container.child_count = 6;
}

static EventResult builder_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    WidgetBuilderData *d = (WidgetBuilderData *)(self + 1);

    if (d->prop_editing) {
        if (ev->type == EVENT_KEY) {
            switch (ev->code) {
                case KEY_ESC: d->prop_editing = false; d->base.dirty = true; return event_result_handled();
                case KEY_ENTER: {
                    if (d->selected_item >= 0 && d->selected_item < d->item_count) {
                        BuilderItem *item = &d->items[d->selected_item];
                        cJSON_AddStringToObject(item->params, "title", d->prop_edit_buf);
                        rebuild_live_widget(item);
                    }
                    d->prop_editing = false;
                    d->base.dirty = true;
                    return event_result_handled();
                }
                case KEY_BACKSPACE:
                    if (d->prop_edit_len > 0) d->prop_edit_buf[--d->prop_edit_len] = '\0';
                    d->base.dirty = true;
                    return event_result_handled();
                case KEY_CHAR:
                    if (d->prop_edit_len < 254 && ev->ch >= 32) {
                        d->prop_edit_buf[d->prop_edit_len++] = ev->ch;
                        d->prop_edit_buf[d->prop_edit_len] = '\0';
                    }
                    d->base.dirty = true;
                    return event_result_handled();
                default: return event_result_unhandled();
            }
        }
        return event_result_unhandled();
    }

    if (ev->type == EVENT_MOUSE_DRAG_START) {
        int mx = ev->x, my = ev->y;
        for (int i = d->item_count - 1; i >= 0; i--) {
            BuilderItem *item = &d->items[i];
            int ix = (int)(item->x * d->scale) + d->canvas_ox;
            int iy = (int)(item->y * d->scale) + d->canvas_oy;
            int iw = (int)(item->w * d->scale);
            int ih = (int)(item->h * d->scale);
            if (mx >= ix && mx < ix + iw && my >= iy && my < iy + ih) {
                d->dragging = true;
                d->drag_item = i;
                d->drag_start_x = mx;
                d->drag_start_y = my;
                d->drag_orig_x = item->x;
                d->drag_orig_y = item->y;
                d->selected_item = i;
                d->base.dirty = true;
                return event_result_handled();
            }
        }
        return event_result_unhandled();
    }

    if (ev->type == EVENT_MOUSE_DRAG_MOVE && d->dragging) {
        if (d->drag_item >= 0 && d->drag_item < d->item_count) {
            BuilderItem *item = &d->items[d->drag_item];
            item->x = d->drag_orig_x + (int)((ev->x - d->drag_start_x) / d->scale);
            item->y = d->drag_orig_y + (int)((ev->y - d->drag_start_y) / d->scale);
            d->base.dirty = true;
        }
        return event_result_handled();
    }

    if (ev->type == EVENT_MOUSE_DRAG_END && d->dragging) {
        d->dragging = false;
        d->drag_item = -1;
        builder_push_undo(d, "move widget");
        d->base.dirty = true;
        return event_result_handled();
    }

    if (ev->type == EVENT_MOUSE_BUTTON && ev->mouse_state == MOUSE_PRESS) {
        int mx = ev->x, my = ev->y;
        for (int i = d->item_count - 1; i >= 0; i--) {
            BuilderItem *item = &d->items[i];
            int ix = (int)(item->x * d->scale) + d->canvas_ox;
            int iy = (int)(item->y * d->scale) + d->canvas_oy;
            int iw = (int)(item->w * d->scale);
            int ih = (int)(item->h * d->scale);
            if (mx >= ix && mx < ix + iw && my >= iy && my < iy + ih) {
                d->selected_item = i;
                d->base.dirty = true;
                return event_result_handled();
            }
        }
        return event_result_unhandled();
    }

    if (ev->type != EVENT_KEY) return event_result_unhandled();

    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_F1: d->mode = BMODE_PALETTE; d->base.dirty = true; return event_result_handled();
        case KEY_F2: d->mode = BMODE_CANVAS; d->base.dirty = true; return event_result_handled();
        case KEY_F3: d->mode = BMODE_PALETTE; d->base.dirty = true; return event_result_handled();
        case KEY_F4:
            if (d->selected_item >= 0 && d->selected_item < d->item_count) {
                d->prop_editing = true;
                d->prop_edit_len = 0;
                d->prop_edit_buf[0] = '\0';
                BuilderItem *item = &d->items[d->selected_item];
                cJSON *t = cJSON_GetObjectItem(item->params, "title");
                if (t && t->valuestring) {
                    strncpy(d->prop_edit_buf, t->valuestring, 255);
                    d->prop_edit_len = strlen(d->prop_edit_buf);
                }
            }
            d->base.dirty = true;
            return event_result_handled();
        case KEY_F5:
            if (d->mode == BMODE_LIVE_PREVIEW) d->mode = BMODE_CANVAS;
            else d->mode = BMODE_LIVE_PREVIEW;
            d->base.dirty = true;
            return event_result_handled();

        case KEY_UP:
            if (d->mode == BMODE_CANVAS && d->selected_item >= 0)
                d->items[d->selected_item].y -= 5;
            else if (d->selected_item > 0) d->selected_item--;
            d->base.dirty = true;
            return event_result_handled();
        case KEY_DOWN:
            if (d->mode == BMODE_CANVAS && d->selected_item >= 0)
                d->items[d->selected_item].y += 5;
            else if (d->selected_item + 1 < d->item_count) d->selected_item++;
            d->base.dirty = true;
            return event_result_handled();
        case KEY_LEFT:
            if (d->mode == BMODE_CANVAS && d->selected_item >= 0)
                d->items[d->selected_item].x -= 5;
            d->base.dirty = true;
            return event_result_handled();
        case KEY_RIGHT:
            if (d->mode == BMODE_CANVAS && d->selected_item >= 0)
                d->items[d->selected_item].x += 5;
            d->base.dirty = true;
            return event_result_handled();

        case KEY_ENTER:
            if (d->palette_idx >= 0 && d->palette_idx < d->palette_count) {
                const char *type = d->palette_types[d->palette_idx];
                d->item_count++;
                d->items = realloc(d->items, d->item_count * sizeof(BuilderItem));
                BuilderItem *item = &d->items[d->item_count - 1];
                memset(item, 0, sizeof(*item));
                item->widget_type = strdup(type);
                item->title = strdup(type);
                item->x = 10 + (d->item_count % 5) * 60;
                item->y = 10 + (d->item_count / 5) * 60;
                item->w = 200;
                item->h = 120;
                item->params = cJSON_CreateObject();
                cJSON_AddStringToObject(item->params, "title", type);
                cJSON_AddStringToObject(item->params, "message", "");
                item->live_widget = create_live_widget(type, type, item->params);
                d->selected_item = d->item_count - 1;
                builder_push_undo(d, "add widget");
            }
            d->base.dirty = true;
            return event_result_handled();

        case KEY_DELETE:
            if (d->selected_item >= 0 && d->selected_item < d->item_count) {
                if (d->items[d->selected_item].live_widget)
                    widget_destroy(d->items[d->selected_item].live_widget);
                free(d->items[d->selected_item].widget_type);
                free(d->items[d->selected_item].title);
                cJSON_Delete(d->items[d->selected_item].params);
                memmove(&d->items[d->selected_item], &d->items[d->selected_item + 1],
                    (d->item_count - d->selected_item - 1) * sizeof(BuilderItem));
                d->item_count--;
                if (d->selected_item >= d->item_count) d->selected_item = d->item_count - 1;
                builder_push_undo(d, "delete widget");
            }
            d->base.dirty = true;
            return event_result_handled();

        case KEY_CHAR:
            if (ev->ch == 's' || ev->ch == 'S') {
                cJSON *root = cJSON_CreateObject();
                cJSON *widgets = cJSON_CreateArray();
                for (int i = 0; i < d->item_count; i++) {
                    cJSON *w = cJSON_CreateObject();
                    cJSON_AddStringToObject(w, "widget", d->items[i].widget_type);
                    cJSON_AddItemToObject(w, "params", cJSON_Duplicate(d->items[i].params, 1));
                    cJSON_AddNumberToObject(w, "x", d->items[i].x);
                    cJSON_AddNumberToObject(w, "y", d->items[i].y);
                    cJSON_AddNumberToObject(w, "w", d->items[i].w);
                    cJSON_AddNumberToObject(w, "h", d->items[i].h);
                    cJSON_AddItemToArray(widgets, w);
                }
                cJSON_AddItemToObject(root, "widgets", widgets);
                char *json = cJSON_PrintUnformatted(root);
                WidgetResponse resp = { .result = cJSON_CreateString(json), .cancelled = false };
                free(json);
                cJSON_Delete(root);
                return event_result_response(resp);
            }
            if (ev->ch == 26) {
                if (undo_stack_undo(d->undo)) d->base.dirty = true;
                return event_result_handled();
            }
            if (ev->ch == 25) {
                if (undo_stack_redo(d->undo)) d->base.dirty = true;
                return event_result_handled();
            }
            return event_result_unhandled();
        default:
            return event_result_unhandled();
    }
}

static void builder_destroy(Widget *self) {
    WidgetBuilderData *d = (WidgetBuilderData *)(self + 1);
    for (int i = 0; i < d->item_count; i++) {
        free(d->items[i].widget_type);
        free(d->items[i].title);
        if (d->items[i].params) cJSON_Delete(d->items[i].params);
        if (d->items[i].live_widget) widget_destroy(d->items[i].live_widget);
    }
    free(d->items);
    for (int i = 0; i < d->palette_count; i++) free(d->palette_types[i]);
    free(d->palette_types);
    if (d->undo) undo_stack_free(d->undo);
}

Widget *widget_builder_new(void) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(WidgetBuilderData));
    WidgetBuilderData *d = (WidgetBuilderData *)(w + 1);
    d->base.dirty = true;
    d->items = NULL;
    d->item_count = 0;
    d->selected_item = -1;
    d->mode = BMODE_CANVAS;
    d->palette_idx = 0;
    d->canvas_ox = 0;
    d->canvas_oy = 0;
    d->scale = 0.5f;
    d->drag_item = -1;
    d->dragging = false;
    d->prop_editing = false;
    d->undo = undo_stack_new(200);

    int pc = sizeof(builtin_palette) / sizeof(builtin_palette[0]);
    d->palette_count = pc;
    d->palette_types = malloc(pc * sizeof(char *));
    for (int i = 0; i < pc; i++)
        d->palette_types[i] = strdup(builtin_palette[i]);

    w->vtable.render = builder_render;
    w->vtable.handle_event = builder_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = builder_destroy;
    return w;
}