#pragma once
#include "render.h"
#include "cJSON.h"

typedef struct StyleRule {
    char *widget_class, *child_class, *state;
    WidgetStyle style;
    struct StyleRule *next;
} StyleRule;

typedef struct Variable { char *name, *value; } Variable;

typedef struct Theme_s {
    char *name, *extends;
    Variable *vars;
    int var_count;
    StyleRule *rules;
    struct Theme_s *next;
} Theme;

uint32_t parse_color(const char *s);

Theme *theme_load(const char *path);
Theme *theme_default(void);
void theme_free(Theme *t);
WidgetStyle theme_resolve(Theme *t, const char *widget_class, const char *child_class, const char *state);
Theme *theme_load_directory(const char *dir_path);
void theme_add_plugin_overrides(Theme *base, const char *plugin_dir);
void theme_merge_user_override(Theme *base);
void theme_merge_app_override(Theme *base, const char *path);
void theme_merge_accessibility_profile(Theme *base, const char *profile_name);
struct FilResult;
void theme_apply_fil_styles(Theme *t, struct FilResult *fr);