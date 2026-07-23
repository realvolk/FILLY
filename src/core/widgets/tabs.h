#pragma once
#include "core/widget.h"

Widget *tabs_widget_new(const char *title, char **tab_labels, int tab_count, Widget **children, int child_count);