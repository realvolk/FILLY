#pragma once
#include "render.h"

#ifdef FILLY_ACCESSIBILITY

bool accessibility_init(void);
void accessibility_shutdown(void);
void accessibility_push_tree(RenderTree *tree, const char *focused_id);

#else

#define accessibility_init() true
#define accessibility_shutdown() ((void)0)
#define accessibility_push_tree(t, f) ((void)0)

#endif