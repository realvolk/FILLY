#pragma once
#include "core/widget.h"
#include <stdbool.h>

Widget *yesno_widget_new(const char *title, const char *message, bool default_yes);