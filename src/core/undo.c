#include "undo.h"
#include <stdlib.h>
#include <string.h>

UndoStack *undo_stack_new(int max_depth) {
    UndoStack *s = calloc(1, sizeof(UndoStack));
    s->max_depth = max_depth > 0 ? max_depth : 100;
    return s;
}

void undo_stack_free(UndoStack *s) {
    if (!s) return;
    undo_stack_clear(s);
    free(s);
}

void undo_stack_push(UndoStack *s, WidgetAction *action) {
    if (!s || !action) return;
    while (s->current && s->current->next) {
        WidgetAction *tmp = s->current->next;
        s->current->next = NULL;
        tmp->prev = NULL;
        if (tmp->free_data) tmp->free_data(tmp->data);
        free(tmp->description);
        free(tmp);
    }
    if (!s->head) {
        s->head = action;
        s->tail = action;
        s->current = action;
    } else {
        s->current->next = action;
        action->prev = s->current;
        s->current = action;
        s->tail = action;
    }
    s->depth++;
    while (s->depth > s->max_depth) {
        WidgetAction *old = s->head;
        s->head = old->next;
        if (s->head) s->head->prev = NULL;
        if (old->free_data) old->free_data(old->data);
        free(old->description);
        free(old);
        s->depth--;
    }
}

bool undo_stack_undo(UndoStack *s) {
    if (!s || !s->current || !s->current->undo) return false;
    s->current->undo(s->current->data);
    s->current = s->current->prev;
    return true;
}

bool undo_stack_redo(UndoStack *s) {
    if (!s) return false;
    WidgetAction *target = s->current ? s->current->next : s->head;
    if (!target || !target->redo) return false;
    target->redo(target->data);
    s->current = target;
    return true;
}

void undo_stack_clear(UndoStack *s) {
    if (!s) return;
    WidgetAction *a = s->head;
    while (a) {
        WidgetAction *next = a->next;
        if (a->free_data) a->free_data(a->data);
        free(a->description);
        free(a);
        a = next;
    }
    s->head = s->current = s->tail = NULL;
    s->depth = 0;
}

const char *undo_stack_peek_undo(UndoStack *s) {
    if (!s || !s->current) return NULL;
    return s->current->description;
}

const char *undo_stack_peek_redo(UndoStack *s) {
    if (!s) return NULL;
    WidgetAction *target = s->current ? s->current->next : s->head;
    return target ? target->description : NULL;
}