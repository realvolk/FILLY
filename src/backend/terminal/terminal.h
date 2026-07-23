#pragma once
#include "core/backend.h"
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    int tty_fd, orig_flags;
    FILE *tty_stream;
    bool raw_mode, alt_screen, session_active;
    char *tty_path;
} TerminalBackend;

bool terminal_backend_init(TerminalBackend *t, const char *tty_path);
void terminal_backend_destroy(TerminalBackend *t);
Backend *terminal_backend_new(void);