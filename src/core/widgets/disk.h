#pragma once
#include "core/widget.h"
#include "cJSON.h"

Widget *disk_widget_new(const char *title, const char *disk, cJSON *partitions, cJSON *free_space, bool readonly);