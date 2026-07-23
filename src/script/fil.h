#pragma once
#include <stdbool.h>

typedef struct FilResult {
    bool accepted;
    char *error_msg;
    char **set_keys;
    char **set_vals;
    int set_count;
    bool show_widget;
    char **style_widget;
    char **style_props;
    int style_count;
    char **keymap_bindings;
    int keymap_count;
} FilResult;

FilResult *fil_eval(const char *script, const char *(*store_get)(const char *key), const char *current_value);
void fil_result_free(FilResult *r);