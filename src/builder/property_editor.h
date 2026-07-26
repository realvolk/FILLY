#pragma once
#include "project.h"
#include "core/render.h"
#include "core/event.h"
#include <stdbool.h>

typedef struct {
    BuilderProject *project;
    int editing_item_id;
    char field_buf[256];
    int field_len;
    int active_field;
    bool editing;
    int field_count;
    struct {
        char *label;
        char *key;
        int type;
    } *fields;
    int scroll_offset;
} PropertyEditor;

PropertyEditor *prop_editor_new(BuilderProject *p);
void prop_editor_free(PropertyEditor *pe);
void prop_editor_set_item(PropertyEditor *pe, int item_id);
void prop_editor_render(PropertyEditor *pe, RenderTree *out, Rect area);
void prop_editor_key(PropertyEditor *pe, KeyCode code, char ch);
void prop_editor_update_param(PropertyEditor *pe);
void prop_editor_load_fields(PropertyEditor *pe);