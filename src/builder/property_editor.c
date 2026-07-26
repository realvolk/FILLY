#include "property_editor.h"
#include "core/widget.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern Arena *g_session_arena;

PropertyEditor *prop_editor_new(BuilderProject *p) {
    PropertyEditor *pe = calloc(1, sizeof(PropertyEditor));
    pe->project = p;
    pe->editing_item_id = -1;
    pe->active_field = 0;
    return pe;
}

void prop_editor_free(PropertyEditor *pe) {
    if (!pe) return;
    for (int i = 0; i < pe->field_count; i++) {
        free(pe->fields[i].label);
        free(pe->fields[i].key);
    }
    free(pe->fields);
    free(pe);
}

void prop_editor_load_fields(PropertyEditor *pe) {
    for (int i = 0; i < pe->field_count; i++) {
        free(pe->fields[i].label);
        free(pe->fields[i].key);
    }
    free(pe->fields);
    pe->fields = NULL;
    pe->field_count = 0;

    if (pe->editing_item_id < 0) return;
    CanvasItem *item = project_find_item(pe->project, pe->editing_item_id);
    if (!item) return;

    int pcount = 0;
    const ParamDesc *params = widget_get_params(item->widget_type, &pcount);
    if (!params || pcount == 0) return;

    pe->field_count = pcount;
    pe->fields = calloc(pcount, sizeof(*pe->fields));
    for (int i = 0; i < pcount; i++) {
        pe->fields[i].label = strdup(params[i].name);
        pe->fields[i].key = strdup(params[i].name);
        switch (params[i].type) {
            case P_STR:  pe->fields[i].type = 0; break;
            case P_INT:  pe->fields[i].type = 1; break;
            case P_BOOL: pe->fields[i].type = 2; break;
            case P_JSON: pe->fields[i].type = 0; break;
            case P_STRS: pe->fields[i].type = 0; break;
        }
    }
}

void prop_editor_set_item(PropertyEditor *pe, int item_id) {
    pe->editing_item_id = item_id;
    pe->active_field = 0;
    pe->editing = false;
    pe->field_len = 0;
    pe->field_buf[0] = '\0';
    prop_editor_load_fields(pe);
}

void prop_editor_render(PropertyEditor *pe, RenderTree *out, Rect area) {
    memset(out, 0, sizeof(*out));
    out->type = RNODE_CONTAINER;
    out->rect = area;
    out->container.border = BORDER_SINGLE;
    out->container.padding.top = 1;
    out->container.padding.left = 2;
    out->container.padding.right = 2;
    out->container.padding.bottom = 1;

    if (pe->editing_item_id < 0) {
        RenderTree *child = arena_alloc(g_session_arena, sizeof(RenderTree));
        memset(child, 0, sizeof(*child));
        child->type = RNODE_TEXT;
        child->rect = rect_new(0, 0, area.w - 2, 2);
        child->text.content = "No widget selected";
        child->text.align = ALIGN_CENTER;
        out->container.children = child;
        out->container.child_count = 1;
        return;
    }

    CanvasItem *item = project_find_item(pe->project, pe->editing_item_id);
    if (!item) return;

    int visible = area.h - 4;
    if (visible < 1) visible = 1;
    int total = pe->field_count + 2;
    if (pe->scroll_offset > total - visible) pe->scroll_offset = total - visible;
    if (pe->scroll_offset < 0) pe->scroll_offset = 0;

    int child_count = visible + 1;
    if (child_count > total + 1) child_count = total + 1;
    RenderTree *children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree));

    int idx = 0;
    RenderTree *header = &children[idx++];
    memset(header, 0, sizeof(*header));
    header->type = RNODE_TEXT;
    header->rect = rect_new(0, 0, area.w - 4, 1);
    char hdr[128];
    snprintf(hdr, sizeof(hdr), "Properties: %s", item->instance_name);
    header->text.content = arena_strdup(g_session_arena, hdr);
    header->style_class = "text";
    header->state = "title";

    for (int i = pe->scroll_offset; i < pe->field_count && idx < child_count; i++) {
        RenderTree *field = &children[idx++];
        memset(field, 0, sizeof(*field));
        field->type = RNODE_TEXT;
        field->rect = rect_new(0, (idx - 1) * 2, area.w - 4, 2);

        cJSON *val = cJSON_GetObjectItem(item->params, pe->fields[i].key);
        char buf[512];
        if (pe->editing && i == pe->active_field) {
            snprintf(buf, sizeof(buf), "%s: %s_", pe->fields[i].label, pe->field_buf);
        } else if (val) {
            if (val->type == cJSON_String) {
                snprintf(buf, sizeof(buf), "%s: %s", pe->fields[i].label, val->valuestring);
            } else if (val->type == cJSON_Number) {
                snprintf(buf, sizeof(buf), "%s: %d", pe->fields[i].label, val->valueint);
            } else if (val->type == cJSON_True) {
                snprintf(buf, sizeof(buf), "%s: true", pe->fields[i].label);
            } else if (val->type == cJSON_False) {
                snprintf(buf, sizeof(buf), "%s: false", pe->fields[i].label);
            } else {
                char *js = cJSON_PrintUnformatted(val);
                snprintf(buf, sizeof(buf), "%s: %s", pe->fields[i].label, js);
                free(js);
            }
        } else {
            snprintf(buf, sizeof(buf), "%s: (not set)", pe->fields[i].label);
        }
        field->text.content = arena_strdup(g_session_arena, buf);
        field->style_class = (i == pe->active_field) ? "selected" : "normal";
    }

    out->container.children = children;
    out->container.child_count = idx;
}

void prop_editor_key(PropertyEditor *pe, KeyCode code, char ch) {
    if (pe->editing_item_id < 0) return;

    if (pe->editing) {
        switch (code) {
            case KEY_ESC:
                pe->editing = false;
                pe->field_len = 0;
                pe->field_buf[0] = '\0';
                return;
            case KEY_ENTER:
                prop_editor_update_param(pe);
                pe->editing = false;
                return;
            case KEY_UP:
                if (pe->active_field > 0) pe->active_field--;
                pe->editing = false;
                pe->field_len = 0;
                pe->field_buf[0] = '\0';
                return;
            case KEY_DOWN:
                if (pe->active_field < pe->field_count - 1) pe->active_field++;
                pe->editing = false;
                pe->field_len = 0;
                pe->field_buf[0] = '\0';
                return;
            case KEY_BACKSPACE:
                if (pe->field_len > 0) pe->field_buf[--pe->field_len] = '\0';
                return;
            case KEY_CHAR:
                if (pe->field_len < 254 && ch >= 32) {
                    pe->field_buf[pe->field_len++] = ch;
                    pe->field_buf[pe->field_len] = '\0';
                }
                return;
            default:
                return;
        }
    }

    switch (code) {
        case KEY_ENTER:
            pe->editing = true;
            pe->field_len = 0;
            pe->field_buf[0] = '\0';
            if (pe->editing_item_id >= 0) {
                CanvasItem *item = project_find_item(pe->project, pe->editing_item_id);
                if (item && pe->active_field < pe->field_count) {
                    cJSON *val = cJSON_GetObjectItem(item->params, pe->fields[pe->active_field].key);
                    if (val && val->valuestring) {
                        strncpy(pe->field_buf, val->valuestring, 255);
                        pe->field_len = strlen(pe->field_buf);
                    }
                }
            }
            return;
        case KEY_UP:
            if (pe->active_field > 0) pe->active_field--;
            return;
        case KEY_DOWN:
            if (pe->active_field < pe->field_count - 1) pe->active_field++;
            return;
        default:
            return;
    }
}

void prop_editor_update_param(PropertyEditor *pe) {
    if (pe->editing_item_id < 0 || pe->active_field >= pe->field_count) return;
    CanvasItem *item = project_find_item(pe->project, pe->editing_item_id);
    if (!item) return;

    cJSON *val = cJSON_GetObjectItem(item->params, pe->fields[pe->active_field].key);
    if (val) {
        cJSON_DetachItemFromObject(item->params, pe->fields[pe->active_field].key);
        cJSON_Delete(val);
    }
    
    if (pe->field_len > 0) {
        cJSON_AddStringToObject(item->params, pe->fields[pe->active_field].key, pe->field_buf);
    }
}