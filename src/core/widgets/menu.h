#pragma once
#include "core/widget.h"

Widget *menu_widget_new(const char *title, const char *message, char **choices, int count, int default_idx);