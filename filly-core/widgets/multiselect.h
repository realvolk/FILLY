#pragma once
#include "../widget.h"
#include <stdbool.h>

Widget *multiselect_widget_new(const char *title, const char *message, char **choices, int count,
                                const char *placeholder, int min_sel, int max_sel);