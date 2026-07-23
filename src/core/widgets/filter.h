#pragma once
#include "core/widget.h"

Widget *filter_widget_new(const char *title, const char *message, char **choices, int count, const char *placeholder);