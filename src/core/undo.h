#pragma once
#include <stdbool.h>

typedef struct WidgetAction {
    char *description;
    void (*undo)(void *data);
    void (*redo)(void *data);
    void *data;
    void (*free_data)(void *data);
    struct WidgetAction *prev;
    struct WidgetAction *next;
} WidgetAction;

typedef struct {
    WidgetAction *head;
    WidgetAction *current;
    WidgetAction *tail;
    int depth;
    int max_depth;
} UndoStack;

UndoStack *undo_stack_new(int max_depth);
void undo_stack_free(UndoStack *s);
void undo_stack_push(UndoStack *s, WidgetAction *action);
bool undo_stack_undo(UndoStack *s);
bool undo_stack_redo(UndoStack *s);
void undo_stack_clear(UndoStack *s);
const char *undo_stack_peek_undo(UndoStack *s);
const char *undo_stack_peek_redo(UndoStack *s);