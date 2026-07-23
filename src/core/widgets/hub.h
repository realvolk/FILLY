#pragma once
#include "core/widget.h"
#include "cJSON.h"

Widget *hub_widget_new(const char *title, cJSON *categories, cJSON *actions);