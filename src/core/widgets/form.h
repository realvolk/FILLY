#pragma once
#include "core/widget.h"
#include "core/render.h"

Widget *form_widget_new(const char *title, FormField *fields, int field_count, const char *submit_label);