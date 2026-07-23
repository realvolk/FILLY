#pragma once
#include <stdbool.h>

typedef enum {
    KEY_BACKSPACE, KEY_ENTER, KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
    KEY_HOME, KEY_END, KEY_PAGEUP, KEY_PAGEDOWN, KEY_TAB, KEY_BACKTAB,
    KEY_DELETE, KEY_INSERT,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    KEY_CHAR, KEY_ESC, KEY_NULL
} KeyCode;

typedef enum {
    EVENT_NONE, EVENT_KEY, EVENT_RESIZE, EVENT_MOUSE_MOTION, EVENT_MOUSE_BUTTON, EVENT_MOUSE_SCROLL
} EventType;

typedef enum {
    MOUSE_PRESS, MOUSE_RELEASE, MOUSE_MOTION, MOUSE_SCROLL_UP, MOUSE_SCROLL_DOWN
} MouseState;

typedef struct {
    EventType type;
    KeyCode code;
    char ch;
    int w, h;
    int x, y;
    int button;
    MouseState mouse_state;
} Event;