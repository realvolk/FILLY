#include "render.h"
#include <string.h>

void layout_tree(RenderTree *tree, int surface_w, int surface_h, bool is_pixel) {
    if (!tree || surface_w <= 0 || surface_h <= 0) return;
    
    int row_h = is_pixel ? 30 : 1;
    int pad_x = is_pixel ? 30 : 1;
    
    tree->rect.x = 0;
    tree->rect.y = 0;
    tree->rect.w = surface_w;
    tree->rect.h = surface_h;
    
    if (tree->type != RNODE_CONTAINER) return;
    
    int content_w = surface_w - pad_x * 2;
    int content_x = pad_x;
    int current_y = 0;
    
    for (int i = 0; i < tree->u.container.child_count; i++) {
        RenderTree *child = &tree->u.container.children[i];
        
        switch (child->type) {
            case RNODE_TEXT:
                child->rect.x = content_x;
                child->rect.y = current_y;
                child->rect.w = content_w;
                child->rect.h = row_h;
                if (child->u.text.content) {
                    int lines = 1;
                    for (const char *p = child->u.text.content; *p; p++)
                        if (*p == '\n') lines++;
                    child->rect.h = row_h * lines;
                }
                current_y += child->rect.h;
                break;
                
            case RNODE_LIST:
                child->rect.x = content_x;
                child->rect.y = current_y;
                child->rect.w = content_w;
                child->rect.h = row_h * child->u.list.item_count;
                current_y += child->rect.h;
                break;
                
            case RNODE_INPUT:
                child->rect.x = content_x;
                child->rect.y = current_y;
                child->rect.w = content_w;
                child->rect.h = row_h;
                current_y += child->rect.h;
                break;
                
            case RNODE_CHECKBOX:
            case RNODE_TOGGLE:
                child->rect.x = content_x;
                child->rect.y = current_y;
                child->rect.w = content_w;
                child->rect.h = row_h;
                current_y += child->rect.h;
                break;
                
            case RNODE_BADGE:
                child->rect.x = content_x;
                child->rect.y = current_y;
                child->rect.w = is_pixel ? (int)strlen(child->u.badge.text) * 14 + 24 : (int)strlen(child->u.badge.text) + 4;
                child->rect.h = is_pixel ? 34 : 1;
                break;
                
            case RNODE_CALENDAR:
                child->rect.x = content_x;
                child->rect.y = current_y;
                child->rect.w = content_w;
                child->rect.h = row_h * 10;
                current_y += child->rect.h;
                break;
                
            case RNODE_GAUGE:
                child->rect.x = content_x;
                child->rect.y = current_y;
                child->rect.w = content_w;
                child->rect.h = row_h * 2;
                current_y += child->rect.h;
                break;
                
            case RNODE_SEPARATOR:
                child->rect.x = content_x;
                child->rect.y = current_y;
                child->rect.w = content_w;
                child->rect.h = 1;
                current_y += child->rect.h;
                break;
                
            case RNODE_SPINNER:
                child->rect.x = content_x;
                child->rect.y = current_y;
                child->rect.w = content_w;
                child->rect.h = row_h;
                current_y += child->rect.h;
                break;
                
            case RNODE_TABLE:
                child->rect.x = content_x;
                child->rect.y = current_y;
                child->rect.w = content_w;
                child->rect.h = row_h * (child->u.table.row_count + 1);
                current_y += child->rect.h;
                break;
                
            case RNODE_TREE:
                child->rect.x = content_x;
                child->rect.y = current_y;
                child->rect.w = content_w;
                child->rect.h = row_h * child->u.tree.node_count;
                current_y += child->rect.h;
                break;
                
            case RNODE_CONTEXT_MENU:
                child->rect.x = content_x;
                child->rect.y = current_y;
                child->rect.w = content_w;
                child->rect.h = row_h * child->u.context_menu.item_count;
                current_y += child->rect.h;
                break;
                
            case RNODE_FORM:
                child->rect.x = content_x;
                child->rect.y = current_y;
                child->rect.w = content_w;
                child->rect.h = row_h * (child->u.form.field_count + 1);
                current_y += child->rect.h;
                break;
                
            case RNODE_TOAST:
                child->rect.x = content_x;
                child->rect.y = surface_h - row_h * 2;
                child->rect.w = content_w;
                child->rect.h = row_h;
                break;
                
            case RNODE_CONTAINER:
            case RNODE_TABS:
            case RNODE_SPLIT_PANES:
                layout_tree(child, content_w, surface_h - current_y, is_pixel);
                child->rect.x = content_x;
                child->rect.y = current_y;
                current_y += child->rect.h;
                break;
                
            default:
                break;
        }
    }
    
    if (tree->type == RNODE_CONTAINER && tree->u.container.border != BORDER_NONE) {
        tree->rect.w = content_w + pad_x * 2;
        tree->rect.h = current_y;
        tree->rect.x = (surface_w - tree->rect.w) / 2;
        tree->rect.y = (surface_h - tree->rect.h) / 2;
    }
}