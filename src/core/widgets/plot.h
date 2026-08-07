#pragma once
#include "core/widget.h"

Widget *plot_widget_new(const char *type, double *data, int data_count, char **labels, int label_count);