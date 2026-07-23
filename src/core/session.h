#pragma once
#include "widget.h"
#include "backend.h"
#include "clipboard.h"
#include "undo.h"
#include "theme.h"
#include "arena.h"
#include "store.h"

extern Theme *g_active_theme;
extern Store *g_active_store;
extern double session_current_fps;
extern Arena *g_session_arena;

WidgetResponse session_run(Widget *w, Backend *backend);
void session_set_clipboard(ClipboardInterface *ci);
ClipboardInterface *session_get_clipboard(void);
UndoStack *session_get_undo(void);
void session_load_keymap(const char *key, KeyCode code, char ch);
void session_clear_keymap(void);