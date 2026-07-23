#pragma once
#include "core/widget.h"
#include <stdbool.h>

Widget *toggle_widget_new(const char *title, const char *label, bool default_val);