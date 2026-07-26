#pragma once
#include "project.h"
#include "core/render.h"
#include "backend/gcore/renderer.h"
#include "backend/headless/headless.h"
#include <stdbool.h>

typedef enum {
    CANVAS_ACTION_NONE,
    CANVAS_ACTION_MOVE,
    CANVAS_ACTION_RESIZE_N,
    CANVAS_ACTION_RESIZE_S,
    CANVAS_ACTION_RESIZE_E,
    CANVAS_ACTION_RESIZE_W,
    CANVAS_ACTION_RESIZE_NE,
    CANVAS_ACTION_RESIZE_NW,
    CANVAS_ACTION_RESIZE_SE,
    CANVAS_ACTION_RESIZE_SW,
    CANVAS_ACTION_RUBBER_BAND,
    CANVAS_ACTION_PAN
} CanvasAction;

typedef struct {
    BuilderProject *project;
    HeadlessBackend headless;
    PixelBuffer preview;
    int canvas_w;
    int canvas_h;
    float zoom;
    int scroll_x;
    int scroll_y;
    int selected_item;
    int hovered_item;
    CanvasAction action;
    bool dragging;
    int drag_start_x;
    int drag_start_y;
    int drag_orig_x;
    int drag_orig_y;
    int drag_orig_w;
    int drag_orig_h;
    bool show_grid;
    int grid_size;
    bool show_tab_order;
    int *selected_items;
    int selected_count;
    int rubber_x;
    int rubber_y;
    int rubber_w;
    int rubber_h;
    bool context_menu_open;
    int context_menu_x;
    int context_menu_y;
    int context_menu_target;
} Canvas;

Canvas *canvas_new(BuilderProject *p, int w, int h);
void canvas_free(Canvas *c);
void canvas_render(Canvas *c);
void canvas_draw_to_tree(Canvas *c, RenderTree *out, int off_x, int off_y);
bool canvas_mouse_down(Canvas *c, int x, int y, int button);
bool canvas_mouse_move(Canvas *c, int x, int y, bool button_down);
bool canvas_mouse_up(Canvas *c, int x, int y, int button);
bool canvas_mouse_scroll(Canvas *c, int x, int y, int delta);
void canvas_key_move(Canvas *c, int dx, int dy);
void canvas_key_resize(Canvas *c, int dw, int dh);
void canvas_delete_selected(Canvas *c);
void canvas_add_widget(Canvas *c, const char *widget_type, int x, int y);
void canvas_select_item(Canvas *c, int item_id);
void canvas_select_all(Canvas *c);
void canvas_deselect_all(Canvas *c);
void canvas_toggle_tab_order(Canvas *c);
void canvas_toggle_grid(Canvas *c);
void canvas_zoom_in(Canvas *c);
void canvas_zoom_out(Canvas *c);
void canvas_zoom_to_fit(Canvas *c);
void canvas_zoom_at(Canvas *c, float new_zoom, int cx, int cy);
void canvas_align_to_grid(Canvas *c, int item_id);
void canvas_bring_to_front(Canvas *c, int item_id);
void canvas_send_to_back(Canvas *c, int item_id);
void canvas_lock_item(Canvas *c, int item_id);
void canvas_unlock_item(Canvas *c, int item_id);
int canvas_hit_test(Canvas *c, int x, int y);