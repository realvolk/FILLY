#pragma once
#include "../widget.h"

Widget *progress_widget_new(const char *title, char **command, int cmd_count, const char *logfile);