#pragma once
#include "core/widget.h"
#include <stdbool.h>

Widget *checklist_widget_new(const char *title, const char *message, char **choices, int count,
                              int min_sel, int max_sel, char **default_choices, int default_count);