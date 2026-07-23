#include "clipboard.h"
#include <stdlib.h>
#include <string.h>

InternalClipboard *clipboard_internal_new(void) {
    InternalClipboard *c = calloc(1, sizeof(InternalClipboard));
    return c;
}

void clipboard_internal_free(InternalClipboard *c) {
    if (!c) return;
    free(c->text);
    free(c);
}

bool clipboard_internal_has(void *self) {
    InternalClipboard *c = (InternalClipboard *)self;
    return c->text != NULL;
}

char *clipboard_internal_get(void *self) {
    InternalClipboard *c = (InternalClipboard *)self;
    return c->text ? strdup(c->text) : NULL;
}

bool clipboard_internal_set(void *self, const char *text) {
    InternalClipboard *c = (InternalClipboard *)self;
    free(c->text);
    c->text = text ? strdup(text) : NULL;
    return true;
}

ClipboardInterface clipboard_internal_interface(InternalClipboard *c) {
    ClipboardInterface ci = {
        .has_clipboard = clipboard_internal_has,
        .get_clipboard = clipboard_internal_get,
        .set_clipboard = clipboard_internal_set,
        .data = c
    };
    return ci;
}