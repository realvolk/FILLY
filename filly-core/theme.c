#include "theme.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *json_str(cJSON *o, const char *key, const char *def) {
    cJSON *item = cJSON_GetObjectItem(o, key);
    if (item && item->valuestring) return strdup(item->valuestring);
    return strdup(def);
}

Theme *theme_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *buf = malloc(sz + 1);
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return NULL;
    Theme *t = calloc(1, sizeof(Theme));
    t->name = json_str(root, "name", "default");
    cJSON *colors = cJSON_GetObjectItem(root, "colors");
    if (colors) {
        t->title_fg = json_str(colors, "title", "2");
        t->accent_fg = json_str(colors, "accent", "2");
        t->muted_fg = json_str(colors, "muted", "8");
        t->selected_fg = json_str(colors, "selected_fg", "2");
        t->selected_bg = json_str(colors, "selected_bg", "8");
        t->border_fg = json_str(colors, "border", "8");
    }
    cJSON_Delete(root);
    return t;
}

Theme *theme_default(void) {
    Theme *t = calloc(1, sizeof(Theme));
    t->name = strdup("default");
    t->title_fg = strdup("2");
    t->accent_fg = strdup("2");
    t->muted_fg = strdup("8");
    t->selected_fg = strdup("2");
    t->selected_bg = strdup("8");
    t->border_fg = strdup("8");
    return t;
}

void theme_free(Theme *t) {
    if (!t) return;
    free(t->name);
    free(t->title_fg);
    free(t->title_bg);
    free(t->accent_fg);
    free(t->muted_fg);
    free(t->selected_fg);
    free(t->selected_bg);
    free(t->border_fg);
    free(t);
}