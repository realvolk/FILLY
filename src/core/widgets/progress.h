#pragma once
#include "core/widget.h"

Widget *progress_widget_new(const char *title, char **command, int cmd_count, const char *logfile);
void progress_widget_set_yield_fd(Widget *w, int fd);