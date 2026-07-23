#pragma once
#include "core/widget.h"

Widget *table_widget_new(const char *title, char **headers, int header_count, char ***rows, int row_count);