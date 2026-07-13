#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct { int x, y, w, h; } Rect;

typedef enum {
    ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT
} Alignment;

typedef enum {
    BORDER_NONE, BORDER_SINGLE, BORDER_DOUBLE, BORDER_ROUNDED
} BorderStyle;

typedef enum {
    ORIENT_HORIZONTAL, ORIENT_VERTICAL
} Orientation;

typedef struct {
    int top, bottom, left, right;
} EdgeInsets;

typedef struct {
    char *fg;
    char *bg;
    bool bold;
    bool italic;
    bool underline;
} TextStyle;

typedef struct {
    char *label;
    char *meta;
} ListItem;

typedef struct TreeNode_s {
    char *label;
    bool expanded;
    struct TreeNode_s **children;
    int child_count;
} TreeNode;

typedef struct {
    char *label;
    char *widget_type;
    char *value;
    char **choices;
    int choice_count;
    char *placeholder;
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
    union {
        struct {
            char *bg;
            BorderStyle border;
            EdgeInsets padding;
            RenderTree *children;
            int child_count;
        } container;
        struct {
            char *content;
            Alignment align;
            TextStyle style;
        } text;
        struct {
            ListItem *items;
            int item_count;
            int selected;
            TextStyle highlight;
        } list;
        struct {
            char *text;
            int cursor;
            char *placeholder;
            bool masked;
        } input;
        struct {
            char *label;
            bool checked;
            bool focused;
        } checkbox;
        struct {
            char *label;
            bool value;
            bool focused;
        } toggle;
        struct {
            char *message;
            int frame;
        } spinner;
        struct {
            Orientation orientation;
        } separator;
        struct {
            char *text;
            TextStyle style;
        } badge;
        struct {
            int x, y;
        } cursor;
        struct {
            char **headers;
            int header_count;
            char ***rows;
            int row_count;
            int selected_row;
            int selected_col;
            TextStyle highlight;
        } table;
        struct {
            TreeNode *nodes;
            int node_count;
            int *selected_path;
            int path_len;
            TextStyle highlight;
        } tree;
        struct {
            int percent;
            char *label;
            TextStyle style;
        } gauge;
        struct {
            int year;
            int month;
            int selected_day;
            TextStyle highlight;
        } calendar;
        struct {
            FormField *fields;
            int field_count;
            int focused;
            char *submit_label;
        } form;
        struct {
            char **tab_labels;
            int tab_count;
            int active;
            RenderTree *child;
        } tabs;
        struct {
            Orientation orientation;
            int split_position;
            RenderTree *first;
            RenderTree *second;
        } split_panes;
        struct {
            ListItem *items;
            int item_count;
            int selected;
            TextStyle highlight;
        } context_menu;
        struct {
            char *message;
            TextStyle style;
        } toast;
    };
};

TextStyle textstyle_normal(void);
TextStyle textstyle_accent(void);
TextStyle textstyle_muted(void);
TextStyle textstyle_selected(void);
Rect rect_new(int x, int y, int w, int h);
EdgeInsets edgeinsets_zero(void);
ListItem listitem_new(const char *label);