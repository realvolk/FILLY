#pragma once

typedef struct {
    char *name;
    char *title_fg;
    char *title_bg;
    char *accent_fg;
    char *muted_fg;
    char *selected_fg;
    char *selected_bg;
    char *border_fg;
} Theme;

Theme *theme_load(const char *path);
Theme *theme_default(void);
void theme_free(Theme *t);