#pragma once
#include <stdbool.h>
#include <stdint.h>

struct Arena;
struct Theme_s;
struct AnimInstance;

typedef struct { int x, y, w, h; } Rect;

typedef enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT } Alignment;
typedef enum { BORDER_NONE, BORDER_SINGLE, BORDER_DOUBLE, BORDER_ROUNDED,
               BORDER_SOLID, BORDER_DASHED, BORDER_DOTTED } BorderStyle;
typedef enum { ORIENT_HORIZONTAL, ORIENT_VERTICAL } Orientation;
typedef enum { OVERFLOW_VISIBLE, OVERFLOW_CLIP, OVERFLOW_SCROLL } Overflow;
typedef enum { ANCHOR_TOP_LEFT, ANCHOR_TOP_CENTER, ANCHOR_TOP_RIGHT,
               ANCHOR_CENTER_LEFT, ANCHOR_CENTER, ANCHOR_CENTER_RIGHT,
               ANCHOR_BOTTOM_LEFT, ANCHOR_BOTTOM_CENTER, ANCHOR_BOTTOM_RIGHT,
               ANCHOR_ABSOLUTE, ANCHOR_RELATIVE } AnchorMode;
typedef enum { CURSOR_DEFAULT, CURSOR_POINTER, CURSOR_TEXT, CURSOR_MOVE,
               CURSOR_RESIZE_N, CURSOR_RESIZE_S, CURSOR_RESIZE_E, CURSOR_RESIZE_W,
               CURSOR_RESIZE_NE, CURSOR_RESIZE_NW, CURSOR_RESIZE_SE, CURSOR_RESIZE_SW,
               CURSOR_NOT_ALLOWED } CursorStyle;

typedef struct { int top, bottom, left, right; } EdgeInsets;

#define MAX_SHADOWS 4
typedef struct {
    int offset_x, offset_y;
    int blur;
    int spread;
    uint32_t color;
} ShadowLayer;

typedef enum { GRADIENT_NONE, GRADIENT_LINEAR, GRADIENT_RADIAL } GradientType;
typedef struct {
    float offset;
    uint32_t color;
} GradientStop;
#define MAX_GRADIENT_STOPS 8
typedef struct {
    GradientType type;
    float angle;
    float center_x, center_y;
    GradientStop stops[MAX_GRADIENT_STOPS];
    int stop_count;
} Gradient;

typedef struct {
    uint32_t fg_color, bg_color, border_color, accent_color;
    int border_width, border_radius;
    int border_top_width, border_bottom_width, border_left_width, border_right_width;
    uint32_t border_top_color, border_bottom_color, border_left_color, border_right_color;
    BorderStyle border_top_style, border_bottom_style, border_left_style, border_right_style;
    int padding[4], margin[4];
    int min_width, min_height, max_width, max_height;
    char *font_family;
    int font_size, font_weight;
    bool font_italic;
    Alignment text_align;
    float opacity;
    ShadowLayer shadows[MAX_SHADOWS];
    int shadow_count;
    Gradient gradient;
    int backdrop_blur;
    float letter_spacing;
    float line_height;
    int text_transform;
    int transition_ms;
    float scale_x, scale_y;
    float rotation;
    float translate_x, translate_y;
    CursorStyle cursor_style;
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

typedef struct {
    Rect parent_rect;
    int screen_width, screen_height;
    bool is_modal;
    const char *active_profile;
    int depth;
    bool prefers_reduced_motion;
} WidgetContext;

typedef enum {
    RNODE_CONTAINER, RNODE_TEXT, RNODE_LIST, RNODE_INPUT, RNODE_CHECKBOX,
    RNODE_TOGGLE, RNODE_SPINNER, RNODE_SEPARATOR, RNODE_BADGE, RNODE_CURSOR,
    RNODE_TABLE, RNODE_TREE, RNODE_GAUGE, RNODE_CALENDAR, RNODE_FORM,
    RNODE_TABS, RNODE_SPLIT_PANES, RNODE_CONTEXT_MENU, RNODE_TOAST,
    RNODE_FLEX, RNODE_GRID, RNODE_VECTOR, RNODE_RICH_TEXT,
    RNODE_IMAGE, RNODE_CANVAS, RNODE_MARKDOWN, RNODE_PLOT, RNODE_VIDEO
} RenderNodeType;

typedef struct RenderTree_s RenderTree;

struct RenderTree_s {
    RenderNodeType type;
    Rect rect;
    Rect target_rect;
    bool has_target_rect;
    char *style_class, *state;
    bool state_owned;
    WidgetStyle resolved_style, prev_resolved_style;
    long long state_change_time;
    bool dirty;
    int z_index;
    AnchorMode anchor;
    int anchor_dx, anchor_dy;
    char *relative_to;
    Overflow overflow;
    char *tooltip;
    int tab_index;
    bool draggable;
    bool drop_target;
    WidgetContext *context;
    struct { char *role, *label; } accessible;
    struct AnimInstance *active_animations;
    int animation_count;
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
        struct { Orientation orientation; int split_position; int split_position2; RenderTree *first, *second, *third; } split_panes;
        struct { ListItem *items; int item_count, selected; } context_menu;
        struct { char *message; } toast;
        struct { char *direction, *wrap, *justify, *align; RenderTree *children; int child_count; } flex;
        struct { int columns, rows; RenderTree *children; int child_count; } grid;
        struct { char *path; char *fill, *stroke; float stroke_width; } vector;
        struct { char *spans; } rich_text;
        struct { char *source; char *fit; int width, height; } image;
        struct { char *script; int width, height; } canvas;
        struct { char *content; } markdown;
        struct { char *type; double *data; int data_count; char **labels; int label_count; } plot;
        struct { char *source; bool loop; } video;
    } u;
};

static inline uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { return (a<<24)|(r<<16)|(g<<8)|b; }

Rect rect_new(int x, int y, int w, int h);
EdgeInsets edgeinsets_zero(void);
ListItem listitem_new(const char *label);
WidgetStyle widgetstyle_default(void);
void resolve_node_styles(RenderTree *tree, struct Theme_s *theme);
void render_tree_mark_dirty(RenderTree *tree);
void render_tree_free(RenderTree *tree);
void layout_tree(RenderTree *tree, int surface_w, int surface_h, bool is_pixel);