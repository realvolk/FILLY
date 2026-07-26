#include "canvas.h"
#include "core/session.h"
#include "core/theme.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

extern Theme *g_active_theme;
extern Arena *g_session_arena;

static int snap_to_grid(int v, int grid) {
    if (grid <= 0) return v;
    return ((v + grid / 2) / grid) * grid;
}

static int canvas_to_screen_x(Canvas *c, int canvas_x) {
    return (int)((canvas_x - c->scroll_x) * c->zoom);
}

static int canvas_to_screen_y(Canvas *c, int canvas_y) {
    return (int)((canvas_y - c->scroll_y) * c->zoom);
}

static int screen_to_canvas_x(Canvas *c, int screen_x) {
    return (int)(screen_x / c->zoom + c->scroll_x);
}

static int screen_to_canvas_y(Canvas *c, int screen_y) {
    return (int)(screen_y / c->zoom + c->scroll_y);
}

static int get_resize_handle(Canvas *c, CanvasItem *item, int screen_x, int screen_y) {
    int ix = canvas_to_screen_x(c, item->rect.x);
    int iy = canvas_to_screen_y(c, item->rect.y);
    int iw = (int)(item->rect.w * c->zoom);
    int ih = (int)(item->rect.h * c->zoom);
    int margin = 8;

    if (screen_x < ix - margin || screen_x > ix + iw + margin) return 0;
    if (screen_y < iy - margin || screen_y > iy + ih + margin) return 0;

    bool left = screen_x < ix + margin;
    bool right = screen_x > ix + iw - margin;
    bool top = screen_y < iy + margin;
    bool bottom = screen_y > iy + ih - margin;

    if (top && left) return CANVAS_ACTION_RESIZE_NW;
    if (top && right) return CANVAS_ACTION_RESIZE_NE;
    if (bottom && left) return CANVAS_ACTION_RESIZE_SW;
    if (bottom && right) return CANVAS_ACTION_RESIZE_SE;
    if (top) return CANVAS_ACTION_RESIZE_N;
    if (bottom) return CANVAS_ACTION_RESIZE_S;
    if (left) return CANVAS_ACTION_RESIZE_W;
    if (right) return CANVAS_ACTION_RESIZE_E;

    return CANVAS_ACTION_MOVE;
}

Canvas *canvas_new(BuilderProject *p, int w, int h) {
    Canvas *c = calloc(1, sizeof(Canvas));
    c->project = p;
    c->canvas_w = w > 0 ? w : 800;
    c->canvas_h = h > 0 ? h : 600;
    c->zoom = 1.0f;
    c->scroll_x = 0;
    c->scroll_y = 0;
    c->selected_item = -1;
    c->hovered_item = -1;
    c->action = CANVAS_ACTION_NONE;
    c->show_grid = true;
    c->grid_size = 8;
    c->show_tab_order = false;
    c->context_menu_open = false;

    headless_backend_init_pixel(&c->headless, c->canvas_w, c->canvas_h);
    c->preview.pixels = NULL;
    c->preview.width = c->canvas_w;
    c->preview.height = c->canvas_h;
    c->preview.stride = c->canvas_w;
    c->preview.mouse_x = 0;
    c->preview.mouse_y = 0;

    return c;
}

void canvas_free(Canvas *c) {
    if (!c) return;
    free(c->preview.pixels);
    headless_backend_destroy(&c->headless);
    free(c->selected_items);
    free(c);
}

void canvas_render(Canvas *c) {
    if (!c || !c->project) return;

    int w = (int)(c->canvas_w * c->zoom);
    int h = (int)(c->canvas_h * c->zoom);
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    if (!c->preview.pixels || c->preview.width != w || c->preview.height != h) {
        free(c->preview.pixels);
        c->preview.pixels = calloc(w * h, sizeof(uint32_t));
        c->preview.width = w;
        c->preview.height = h;
        c->preview.stride = w;
    }

    for (int row = 0; row < h; row++) {
        uint32_t *line = c->preview.pixels + row * w;
        for (int col = 0; col < w; col++) {
            line[col] = gcore_rgba(26, 26, 46, 255);
        }
    }

    Arena *arena = arena_new(1024 * 1024);
    if (!arena) return;

    RenderTree tree;
    memset(&tree, 0, sizeof(tree));
    tree.type = RNODE_CONTAINER;
    tree.rect = rect_new(0, 0, c->project->root_width, c->project->root_height);
    tree.resolved_style = widgetstyle_default();
    tree.resolved_style.bg_color = gcore_rgba(26, 26, 46, 255);

    int child_count = 0;
    for (int i = 0; i < c->project->item_count; i++) {
        if (c->project->items[i].parent_id == -1 && c->project->items[i].visible) {
            child_count++;
        }
    }

    if (child_count > 0) {
        RenderTree *children = arena_alloc(arena, child_count * sizeof(RenderTree));
        int idx = 0;

        for (int i = 0; i < c->project->item_count; i++) {
            CanvasItem *item = &c->project->items[i];
            if (item->parent_id != -1 || !item->visible) continue;

            RenderTree *child = &children[idx++];
            memset(child, 0, sizeof(*child));
            child->rect = item->rect;
            child->resolved_style = widgetstyle_default();
            child->resolved_style.font_size = 14;
            child->resolved_style.fg_color = gcore_rgba(208, 208, 208, 255);
            child->resolved_style.bg_color = gcore_rgba(26, 26, 46, 255);
            child->resolved_style.border_color = gcore_rgba(42, 42, 74, 255);
            child->resolved_style.accent_color = gcore_rgba(233, 69, 96, 255);
            child->resolved_style.border_radius = 4;
            child->resolved_style.border_width = 1;

            bool is_selected = false;
            for (int s = 0; s < c->selected_count; s++) {
                if (c->selected_items[s] == item->id) {
                    is_selected = true;
                    break;
                }
            }
            if (is_selected) {
                child->resolved_style.border_color = gcore_rgba(233, 69, 96, 255);
                child->resolved_style.border_width = 2;
            }

            if (strcmp(item->widget_type, "input") == 0) {
                child->type = RNODE_INPUT;
                cJSON *def = cJSON_GetObjectItem(item->params, "default");
                cJSON *placeholder = cJSON_GetObjectItem(item->params, "placeholder");
                child->input.text = def && def->valuestring ? arena_strdup(arena, def->valuestring) : arena_strdup(arena, "");
                child->input.placeholder = placeholder && placeholder->valuestring ? arena_strdup(arena, placeholder->valuestring) : arena_strdup(arena, "");
                child->input.cursor = 0;
            } else if (strcmp(item->widget_type, "password") == 0) {
                child->type = RNODE_INPUT;
                cJSON *def = cJSON_GetObjectItem(item->params, "default");
                child->input.text = def && def->valuestring ? arena_strdup(arena, def->valuestring) : arena_strdup(arena, "");
                child->input.placeholder = arena_strdup(arena, "••••••••");
                child->input.masked = true;
            } else if (strcmp(item->widget_type, "toggle") == 0) {
                child->type = RNODE_TOGGLE;
                cJSON *label = cJSON_GetObjectItem(item->params, "label");
                cJSON *def = cJSON_GetObjectItem(item->params, "default");
                child->toggle.label = label && label->valuestring ? arena_strdup(arena, label->valuestring) : arena_strdup(arena, "");
                child->toggle.value = def ? def->valueint : false;
            } else if (strcmp(item->widget_type, "checkbox") == 0) {
                child->type = RNODE_CHECKBOX;
                cJSON *label = cJSON_GetObjectItem(item->params, "label");
                cJSON *def = cJSON_GetObjectItem(item->params, "default");
                child->checkbox.label = label && label->valuestring ? arena_strdup(arena, label->valuestring) : arena_strdup(arena, "");
                child->checkbox.checked = def ? def->valueint : false;
            } else if (strcmp(item->widget_type, "menu") == 0) {
                child->type = RNODE_LIST;
                cJSON *choices = cJSON_GetObjectItem(item->params, "choices");
                if (choices && cJSON_IsArray(choices)) {
                    child->list.item_count = cJSON_GetArraySize(choices);
                    child->list.items = arena_alloc(arena, child->list.item_count * sizeof(ListItem));
                    for (int j = 0; j < child->list.item_count; j++) {
                        cJSON *ch = cJSON_GetArrayItem(choices, j);
                        child->list.items[j].label = arena_strdup(arena, ch && ch->valuestring ? ch->valuestring : "");
                    }
                } else {
                    child->list.item_count = 1;
                    child->list.items = arena_alloc(arena, sizeof(ListItem));
                    child->list.items[0].label = arena_strdup(arena, "No items");
                }
                child->list.selected = 0;
            } else if (strcmp(item->widget_type, "msg") == 0) {
                child->type = RNODE_TEXT;
                cJSON *title = cJSON_GetObjectItem(item->params, "title");
                cJSON *message = cJSON_GetObjectItem(item->params, "message");
                char buf[512];
                snprintf(buf, sizeof(buf), "%s\n%s",
                    title && title->valuestring ? title->valuestring : "",
                    message && message->valuestring ? message->valuestring : "");
                child->text.content = arena_strdup(arena, buf);
            } else if (strcmp(item->widget_type, "button") == 0) {
                child->type = RNODE_TEXT;
                cJSON *title = cJSON_GetObjectItem(item->params, "title");
                child->text.content = arena_strdup(arena, title && title->valuestring ? title->valuestring : "Button");
                child->resolved_style.bg_color = gcore_rgba(233, 69, 96, 255);
                child->resolved_style.fg_color = gcore_rgba(255, 255, 255, 255);
                child->resolved_style.border_radius = 4;
            } else if (strcmp(item->widget_type, "progress") == 0) {
                child->type = RNODE_GAUGE;
                cJSON *percent = cJSON_GetObjectItem(item->params, "percent");
                cJSON *title = cJSON_GetObjectItem(item->params, "title");
                child->gauge.percent = percent ? percent->valueint : 50;
                child->gauge.label = arena_strdup(arena, title && title->valuestring ? title->valuestring : "");
            } else if (strcmp(item->widget_type, "separator") == 0) {
                child->type = RNODE_SEPARATOR;
                child->separator.orientation = ORIENT_HORIZONTAL;
            } else if (strcmp(item->widget_type, "spinner") == 0) {
                child->type = RNODE_SPINNER;
                cJSON *message = cJSON_GetObjectItem(item->params, "message");
                child->spinner.message = arena_strdup(arena, message && message->valuestring ? message->valuestring : "Loading...");
                child->spinner.frame = 0;
            } else if (strcmp(item->widget_type, "badge") == 0) {
                child->type = RNODE_BADGE;
                cJSON *text = cJSON_GetObjectItem(item->params, "text");
                child->badge.text = arena_strdup(arena, text && text->valuestring ? text->valuestring : "");
            } else if (strcmp(item->widget_type, "calendar") == 0) {
                child->type = RNODE_CALENDAR;
            } else if (strcmp(item->widget_type, "gauge") == 0) {
                child->type = RNODE_GAUGE;
                cJSON *percent = cJSON_GetObjectItem(item->params, "percent");
                cJSON *label = cJSON_GetObjectItem(item->params, "label");
                child->gauge.percent = percent ? percent->valueint : 0;
                child->gauge.label = arena_strdup(arena, label && label->valuestring ? label->valuestring : "");
            } else if (strcmp(item->widget_type, "notification") == 0) {
                child->type = RNODE_TOAST;
                cJSON *message = cJSON_GetObjectItem(item->params, "message");
                child->toast.message = arena_strdup(arena, message && message->valuestring ? message->valuestring : "");
            } else {
                child->type = RNODE_TEXT;
                child->text.content = arena_strdup(arena, item->instance_name);
            }

            if (c->show_tab_order && item->tab_index > 0) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", item->tab_index);
                child->text.content = arena_strdup(arena, buf);
                child->resolved_style.fg_color = gcore_rgba(233, 69, 96, 255);
            }
        }

        tree.container.children = children;
        tree.container.child_count = child_count;
    }

    if (g_active_theme) resolve_node_styles(&tree, g_active_theme);
    gcore_render_tree_to_pixels(&tree, &c->preview, g_active_theme ? g_active_theme : NULL, arena);

    if (c->show_grid) {
        int grid_w = (int)(c->grid_size * c->zoom);
        if (grid_w < 2) grid_w = 2;
        for (int x = 0; x < w; x += grid_w) {
            for (int row = 0; row < h && x < w; row++) {
                c->preview.pixels[row * w + x] = gcore_rgba(42, 42, 74, 40);
            }
        }
        for (int y = 0; y < h; y += grid_w) {
            uint32_t *line = c->preview.pixels + y * w;
            for (int col = 0; col < w; col++) {
                line[col] = gcore_rgba(42, 42, 74, 40);
            }
        }
    }

    if (c->action == CANVAS_ACTION_RUBBER_BAND && c->dragging) {
        int rx = c->rubber_x < c->drag_start_x ? c->rubber_x : c->drag_start_x;
        int ry = c->rubber_y < c->drag_start_y ? c->rubber_y : c->drag_start_y;
        int rw = abs(c->rubber_x - c->drag_start_x);
        int rh = abs(c->rubber_y - c->drag_start_y);
        for (int row = ry; row < ry + rh && row < h; row++) {
            uint32_t *line = c->preview.pixels + row * w;
            for (int col = rx; col < rx + rw && col < w; col++) {
                if (row == ry || row == ry + rh - 1 || col == rx || col == rx + rw - 1) {
                    line[col] = gcore_rgba(233, 69, 96, 255);
                }
            }
        }
    }

    arena_free(arena);
}

void canvas_draw_to_tree(Canvas *c, RenderTree *out, int off_x, int off_y) {
    if (!c || !out) return;
    canvas_render(c);
    out->type = RNODE_CONTAINER;
    out->rect = rect_new(off_x, off_y, c->canvas_w, c->canvas_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();

    int h = c->preview.height < c->canvas_h ? c->preview.height : c->canvas_h;
    int w = c->preview.width < c->canvas_w ? c->preview.width : c->canvas_w;
    int lines = h;
    if (lines < 1) lines = 1;

    RenderTree *children = arena_alloc(g_session_arena, lines * sizeof(RenderTree));
    for (int row = 0; row < lines; row++) {
        RenderTree *line = &children[row];
        memset(line, 0, sizeof(*line));
        line->type = RNODE_TEXT;
        line->rect = rect_new(1, row + 1, w, 1);
        char *row_buf = arena_alloc(g_session_arena, w + 1);
        for (int col = 0; col < w; col++) {
            uint32_t px = c->preview.pixels[row * c->preview.width + col];
            uint8_t r = (px >> 16) & 0xFF;
            uint8_t g = (px >> 8) & 0xFF;
            uint8_t b = px & 0xFF;
            int gray = (r + g + b) / 3;
            if (gray < 32) row_buf[col] = ' ';
            else if (gray < 64) row_buf[col] = '.';
            else if (gray < 96) row_buf[col] = ':';
            else if (gray < 128) row_buf[col] = '=';
            else if (gray < 160) row_buf[col] = '+';
            else if (gray < 192) row_buf[col] = '*';
            else if (gray < 224) row_buf[col] = '#';
            else row_buf[col] = '@';
        }
        row_buf[w] = '\0';
        line->text.content = row_buf;
    }
    out->container.children = children;
    out->container.child_count = lines;
}

int canvas_hit_test(Canvas *c, int screen_x, int screen_y) {
    if (!c) return -1;
    for (int i = c->project->item_count - 1; i >= 0; i--) {
        CanvasItem *item = &c->project->items[i];
        if (item->locked || !item->visible) continue;
        int ix = canvas_to_screen_x(c, item->rect.x);
        int iy = canvas_to_screen_y(c, item->rect.y);
        int iw = (int)(item->rect.w * c->zoom);
        int ih = (int)(item->rect.h * c->zoom);
        if (screen_x >= ix && screen_x < ix + iw && screen_y >= iy && screen_y < iy + ih) {
            return item->id;
        }
    }
    return -1;
}

bool canvas_mouse_down(Canvas *c, int screen_x, int screen_y, int button) {
    if (!c) return false;

    if (c->context_menu_open) {
        c->context_menu_open = false;
        return true;
    }

    if (button == 3) {
        int hit = canvas_hit_test(c, screen_x, screen_y);
        c->context_menu_open = true;
        c->context_menu_x = screen_x;
        c->context_menu_y = screen_y;
        c->context_menu_target = hit;
        return true;
    }

    if (button == 2) {
        c->action = CANVAS_ACTION_PAN;
        c->drag_start_x = screen_x;
        c->drag_start_y = screen_y;
        c->dragging = true;
        return true;
    }

    int hit = canvas_hit_test(c, screen_x, screen_y);
    if (hit >= 0) {
        CanvasItem *item = project_find_item(c->project, hit);
        if (!item) return false;

        bool already_selected = false;
        for (int i = 0; i < c->selected_count; i++) {
            if (c->selected_items[i] == hit) {
                already_selected = true;
                break;
            }
        }

        if (!already_selected) {
            canvas_deselect_all(c);
            canvas_select_item(c, hit);
        }

        c->action = get_resize_handle(c, item, screen_x, screen_y);
        c->drag_start_x = screen_x;
        c->drag_start_y = screen_y;
        c->drag_orig_x = item->rect.x;
        c->drag_orig_y = item->rect.y;
        c->drag_orig_w = item->rect.w;
        c->drag_orig_h = item->rect.h;
        c->dragging = true;
        return true;
    }

    c->action = CANVAS_ACTION_RUBBER_BAND;
    c->drag_start_x = screen_x;
    c->drag_start_y = screen_y;
    c->rubber_x = screen_x;
    c->rubber_y = screen_y;
    c->dragging = true;
    return true;
}

bool canvas_mouse_move(Canvas *c, int screen_x, int screen_y, bool button_down) {
    if (!c) return false;

    c->hovered_item = canvas_hit_test(c, screen_x, screen_y);

    if (!c->dragging) {
        return c->hovered_item >= 0;
    }

    if (!button_down) {
        c->dragging = false;
        c->action = CANVAS_ACTION_NONE;
        return false;
    }

    int dx = screen_x - c->drag_start_x;
    int dy = screen_y - c->drag_start_y;

    if (c->action == CANVAS_ACTION_PAN) {
        c->scroll_x -= dx;
        c->scroll_y -= dy;
        c->drag_start_x = screen_x;
        c->drag_start_y = screen_y;
        return true;
    }

    if (c->action == CANVAS_ACTION_RUBBER_BAND) {
        c->rubber_x = screen_x;
        c->rubber_y = screen_y;
        return true;
    }

    for (int i = 0; i < c->selected_count; i++) {
        CanvasItem *item = project_find_item(c->project, c->selected_items[i]);
        if (!item) continue;

        int canvas_dx = (int)(dx / c->zoom);
        int canvas_dy = (int)(dy / c->zoom);

        if (c->action == CANVAS_ACTION_MOVE) {
            item->rect.x = c->drag_orig_x + canvas_dx;
            item->rect.y = c->drag_orig_y + canvas_dy;
            if (c->show_grid) {
                item->rect.x = snap_to_grid(item->rect.x, c->grid_size);
                item->rect.y = snap_to_grid(item->rect.y, c->grid_size);
            }
        } else if (c->action == CANVAS_ACTION_RESIZE_N) {
            item->rect.y = c->drag_orig_y + canvas_dy;
            item->rect.h = c->drag_orig_h - canvas_dy;
        } else if (c->action == CANVAS_ACTION_RESIZE_S) {
            item->rect.h = c->drag_orig_h + canvas_dy;
        } else if (c->action == CANVAS_ACTION_RESIZE_E) {
            item->rect.w = c->drag_orig_w + canvas_dx;
        } else if (c->action == CANVAS_ACTION_RESIZE_W) {
            item->rect.x = c->drag_orig_x + canvas_dx;
            item->rect.w = c->drag_orig_w - canvas_dx;
        } else if (c->action == CANVAS_ACTION_RESIZE_NE) {
            item->rect.y = c->drag_orig_y + canvas_dy;
            item->rect.h = c->drag_orig_h - canvas_dy;
            item->rect.w = c->drag_orig_w + canvas_dx;
        } else if (c->action == CANVAS_ACTION_RESIZE_NW) {
            item->rect.y = c->drag_orig_y + canvas_dy;
            item->rect.h = c->drag_orig_h - canvas_dy;
            item->rect.x = c->drag_orig_x + canvas_dx;
            item->rect.w = c->drag_orig_w - canvas_dx;
        } else if (c->action == CANVAS_ACTION_RESIZE_SE) {
            item->rect.h = c->drag_orig_h + canvas_dy;
            item->rect.w = c->drag_orig_w + canvas_dx;
        } else if (c->action == CANVAS_ACTION_RESIZE_SW) {
            item->rect.h = c->drag_orig_h + canvas_dy;
            item->rect.x = c->drag_orig_x + canvas_dx;
            item->rect.w = c->drag_orig_w - canvas_dx;
        }

        if (item->rect.w < 20) item->rect.w = 20;
        if (item->rect.h < 20) item->rect.h = 20;
    }

    return true;
}

void canvas_toggle_select(Canvas *c, int item_id) {
    for (int i = 0; i < c->selected_count; i++) {
        if (c->selected_items[i] == item_id) {
            memmove(&c->selected_items[i], &c->selected_items[i + 1],
                    (c->selected_count - i - 1) * sizeof(int));
            c->selected_count--;
            if (c->selected_count == 0) {
                c->selected_item = -1;
            } else {
                c->selected_item = c->selected_items[c->selected_count - 1];
            }
            return;
        }
    }
    c->selected_count++;
    c->selected_items = realloc(c->selected_items, c->selected_count * sizeof(int));
    c->selected_items[c->selected_count - 1] = item_id;
    c->selected_item = item_id;
}


bool canvas_mouse_up(Canvas *c, int screen_x, int screen_y, int button) {
    (void)screen_x;
    (void)screen_y;
    (void)button;
    if (!c) return false;

    if (c->action == CANVAS_ACTION_RUBBER_BAND && c->dragging) {
        int rx = c->rubber_x < c->drag_start_x ? c->rubber_x : c->drag_start_x;
        int ry = c->rubber_y < c->drag_start_y ? c->rubber_y : c->drag_start_y;
        int rw = abs(c->rubber_x - c->drag_start_x);
        int rh = abs(c->rubber_y - c->drag_start_y);

        if (rw > 4 || rh > 4) {
            canvas_deselect_all(c);
            for (int i = 0; i < c->project->item_count; i++) {
                CanvasItem *item = &c->project->items[i];
                if (item->locked || !item->visible) continue;
                int ix = canvas_to_screen_x(c, item->rect.x);
                int iy = canvas_to_screen_y(c, item->rect.y);
                int iw = (int)(item->rect.w * c->zoom);
                int ih = (int)(item->rect.h * c->zoom);
                if (ix + iw >= rx && ix <= rx + rw && iy + ih >= ry && iy <= ry + rh) {
                    canvas_toggle_select(c, item->id);
                }
            }
        } else {
            canvas_deselect_all(c);
        }
    }

    c->dragging = false;
    c->action = CANVAS_ACTION_NONE;
    return true;
}

bool canvas_mouse_scroll(Canvas *c, int x, int y, int delta) {
    if (!c) return false;
    float new_zoom = c->zoom * (delta > 0 ? 1.1f : 0.9f);
    canvas_zoom_at(c, new_zoom, x, y);
    return true;
}

void canvas_key_move(Canvas *c, int dx, int dy) {
    for (int i = 0; i < c->selected_count; i++) {
        CanvasItem *item = project_find_item(c->project, c->selected_items[i]);
        if (!item || item->locked) continue;
        item->rect.x += dx;
        item->rect.y += dy;
        if (c->show_grid) {
            item->rect.x = snap_to_grid(item->rect.x, c->grid_size);
            item->rect.y = snap_to_grid(item->rect.y, c->grid_size);
        }
    }
}

void canvas_key_resize(Canvas *c, int dw, int dh) {
    for (int i = 0; i < c->selected_count; i++) {
        CanvasItem *item = project_find_item(c->project, c->selected_items[i]);
        if (!item || item->locked) continue;
        item->rect.w += dw;
        item->rect.h += dh;
        if (item->rect.w < 20) item->rect.w = 20;
        if (item->rect.h < 20) item->rect.h = 20;
    }
}

void canvas_delete_selected(Canvas *c) {
    for (int i = 0; i < c->selected_count; i++) {
        project_remove_item(c->project, c->selected_items[i]);
    }
    c->selected_count = 0;
    free(c->selected_items);
    c->selected_items = NULL;
    c->selected_item = -1;
}

void canvas_add_widget(Canvas *c, const char *widget_type, int screen_x, int screen_y) {
    if (!c || !widget_type) return;
    int cx = screen_to_canvas_x(c, screen_x);
    int cy = screen_to_canvas_y(c, screen_y);
    CanvasItem *item = project_add_item(c->project, widget_type, cx, cy, 200, 120);
    if (item && c->show_grid) {
        item->rect.x = snap_to_grid(item->rect.x, c->grid_size);
        item->rect.y = snap_to_grid(item->rect.y, c->grid_size);
    }
}

void canvas_select_item(Canvas *c, int item_id) {
    c->selected_item = item_id;
    c->selected_count = 1;
    c->selected_items = realloc(c->selected_items, sizeof(int));
    c->selected_items[0] = item_id;
}

void canvas_select_all(Canvas *c) {
    free(c->selected_items);
    c->selected_count = c->project->item_count;
    c->selected_items = malloc(c->selected_count * sizeof(int));
    for (int i = 0; i < c->project->item_count; i++) {
        c->selected_items[i] = c->project->items[i].id;
    }
    if (c->selected_count > 0) c->selected_item = c->selected_items[0];
}

void canvas_deselect_all(Canvas *c) {
    c->selected_item = -1;
    free(c->selected_items);
    c->selected_items = NULL;
    c->selected_count = 0;
}

void canvas_toggle_tab_order(Canvas *c) {
    c->show_tab_order = !c->show_tab_order;
}

void canvas_toggle_grid(Canvas *c) {
    c->show_grid = !c->show_grid;
}

void canvas_zoom_in(Canvas *c) {
    canvas_zoom_at(c, c->zoom * 1.25f, c->canvas_w / 2, c->canvas_h / 2);
}

void canvas_zoom_out(Canvas *c) {
    canvas_zoom_at(c, c->zoom * 0.8f, c->canvas_w / 2, c->canvas_h / 2);
}

void canvas_zoom_to_fit(Canvas *c) {
    if (!c || c->project->root_width <= 0 || c->project->root_height <= 0) return;
    float zx = (float)c->canvas_w / c->project->root_width;
    float zy = (float)c->canvas_h / c->project->root_height;
    float new_zoom = zx < zy ? zx : zy;
    if (new_zoom < 0.1f) new_zoom = 0.1f;
    if (new_zoom > 5.0f) new_zoom = 5.0f;
    c->zoom = new_zoom;
    c->scroll_x = 0;
    c->scroll_y = 0;
}

void canvas_zoom_at(Canvas *c, float new_zoom, int cx, int cy) {
    if (new_zoom < 0.1f) new_zoom = 0.1f;
    if (new_zoom > 5.0f) new_zoom = 5.0f;
    float ratio = new_zoom / c->zoom;
    c->scroll_x = (int)(cx - ratio * (cx - c->scroll_x));
    c->scroll_y = (int)(cy - ratio * (cy - c->scroll_y));
    c->zoom = new_zoom;
}

void canvas_align_to_grid(Canvas *c, int item_id) {
    CanvasItem *item = project_find_item(c->project, item_id);
    if (!item || c->grid_size <= 0) return;
    item->rect.x = snap_to_grid(item->rect.x, c->grid_size);
    item->rect.y = snap_to_grid(item->rect.y, c->grid_size);
}

void canvas_bring_to_front(Canvas *c, int item_id) {
    for (int i = 0; i < c->project->item_count; i++) {
        if (c->project->items[i].id == item_id && i < c->project->item_count - 1) {
            CanvasItem tmp = c->project->items[i];
            memmove(&c->project->items[i], &c->project->items[i + 1],
                    (c->project->item_count - i - 1) * sizeof(CanvasItem));
            c->project->items[c->project->item_count - 1] = tmp;
            break;
        }
    }
}

void canvas_send_to_back(Canvas *c, int item_id) {
    for (int i = 0; i < c->project->item_count; i++) {
        if (c->project->items[i].id == item_id && i > 0) {
            CanvasItem tmp = c->project->items[i];
            memmove(&c->project->items[1], &c->project->items[0],
                    i * sizeof(CanvasItem));
            c->project->items[0] = tmp;
            break;
        }
    }
}

void canvas_lock_item(Canvas *c, int item_id) {
    CanvasItem *item = project_find_item(c->project, item_id);
    if (item) item->locked = true;
}

void canvas_unlock_item(Canvas *c, int item_id) {
    CanvasItem *item = project_find_item(c->project, item_id);
    if (item) item->locked = false;
}
