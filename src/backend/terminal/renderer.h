#pragma once
#include "core/render.h"

void render_tree_to_buf(RenderTree *tree, int off_x, int off_y, int max_w, int max_h, char *buf, int buf_sz);