#pragma once
#include <stdbool.h>
#include <stdint.h>

struct Arena;
typedef struct Theme_s Theme;

typedef struct { int x, y, w, h; } Rect;

typedef enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT } Alignment;
typedef enum { BORDER_NONE, BORDER_SINGLE, BORDER_DOUBLE, BORDER_ROUNDED } BorderStyle;
typedef enum { ORIENT_HORIZONTAL, ORIENT_VERTICAL } Orientation;

typedef struct { int top, bottom, left, right; } EdgeInsets;

typedef struct {
    uint32_t fg_color, bg_color, border_color, accent_color;
    int border_width, border_radius;
    int padding[4], margin[4];
    int min_width, min_height, max_width, max_height;
    char *font_family;
    int font_size, font_weight;
    bool font_italic;
    Alignment text_align;
    float opacity;
    int shadow_offset_x, shadow_offset_y, shadow_blur;
    uint32_t shadow_color;
    bool bg_gradient;
    uint32_t bg_gradient_to;
    int bg_gradient_direction, transition_ms;
} WidgetStyle;

typedef struct { char *fg, *bg; bool bold, italic, underline; } TextStyle;
typedef struct { char *label, *meta; } ListItem;

typedef struct TreeNode_s {
    char *label; bool expanded;
    struct TreeNode_s **children; int child_count;
} TreeNode;

typedef struct {
    char *label, *widget_type, *value, **choices, *placeholder;
    int choice_count;
} FormField;

typedef enum {
    RNODE_CONTAINER, RNODE_TEXT, RNODE_LIST, RNODE_INPUT, RNODE_CHECKBOX,
    RNODE_TOGGLE, RNODE_SPINNER, RNODE_SEPARATOR, RNODE_BADGE, RNODE_CURSOR,
    RNODE_TABLE, RNODE_TREE, RNODE_GAUGE, RNODE_CALENDAR, RNODE_FORM,
    RNODE_TABS, RNODE_SPLIT_PANES, RNODE_CONTEXT_MENU, RNODE_TOAST
} RenderNodeType;

typedef struct RenderTree_s RenderTree;

struct RenderTree_s {
    RenderNodeType type;
    Rect rect;
    char *style_class, *state;
    WidgetStyle resolved_style, prev_resolved_style;
    long long state_change_time;
    bool dirty;
    struct { char *role, *label; } accessible;
    union {
        struct { char *bg; BorderStyle border; EdgeInsets padding; RenderTree *children; int child_count; } container;
        struct { char *content; Alignment align; } text;
        struct { ListItem *items; int item_count, selected; } list;
        struct { char *text, *placeholder; int cursor; bool masked; } input;
        struct { char *label; bool checked, focused; } checkbox;
        struct { char *label; bool value, focused; } toggle;
        struct { char *message; int frame; } spinner;
        struct { Orientation orientation; } separator;
        struct { char *text; } badge;
        struct { int x, y; } cursor;
        struct { char **headers; int header_count; char ***rows; int row_count, selected_row, selected_col; } table;
        struct { TreeNode *nodes; int node_count, *selected_path, path_len; } tree;
        struct { int percent; char *label; } gauge;
        struct { int year, month, selected_day; } calendar;
        struct { FormField *fields; int field_count, focused; char *submit_label; } form;
        struct { char **tab_labels; int tab_count, active; RenderTree *child; } tabs;
        struct { Orientation orientation; int split_position; RenderTree *first, *second; } split_panes;
        struct { ListItem *items; int item_count, selected; } context_menu;
        struct { char *message; } toast;
    };
};

static inline uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { return (a<<24)|(r<<16)|(g<<8)|b; }

Rect rect_new(int x, int y, int w, int h);
EdgeInsets edgeinsets_zero(void);
ListItem listitem_new(const char *label);
WidgetStyle widgetstyle_default(void);
void resolve_node_styles(RenderTree *tree, Theme *theme);
void render_tree_mark_dirty(RenderTree *tree);
void render_tree_free(RenderTree *tree);