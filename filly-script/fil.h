#pragma once
#include <stdbool.h>

typedef struct FilResult {
    bool accepted;
    char *error_msg;
    char **set_keys;
    char **set_vals;
    int set_count;
    bool show_widget;
} FilResult;

FilResult *fil_eval(const char *script, const char *(*store_get)(const char *key), const char *current_value);
void fil_result_free(FilResult *r);