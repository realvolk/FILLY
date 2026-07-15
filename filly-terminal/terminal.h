#pragma once
#include "../filly-core/backend.h"
#include <stdbool.h>

typedef struct {
    int tty_fd;
    int orig_flags;
    bool raw_mode;
    bool alt_screen;
    bool session_active;
    char *tty_path;
} TerminalBackend;

bool terminal_backend_init(TerminalBackend *t, const char *tty_path);
void terminal_backend_destroy(TerminalBackend *t);
Backend *terminal_backend_new(void);