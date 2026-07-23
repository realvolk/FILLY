#pragma once
#include <stdbool.h>

typedef struct {
    bool (*has_clipboard)(void *self);
    char *(*get_clipboard)(void *self);
    bool (*set_clipboard)(void *self, const char *text);
    void *data;
} ClipboardInterface;

typedef struct { char *text; } InternalClipboard;

InternalClipboard *clipboard_internal_new(void);
void clipboard_internal_free(InternalClipboard *c);
bool clipboard_internal_has(void *self);
char *clipboard_internal_get(void *self);
bool clipboard_internal_set(void *self, const char *text);
ClipboardInterface clipboard_internal_interface(InternalClipboard *c);