#pragma once
#include "../widget.h"
#include "../render.h"

Widget *form_widget_new(const char *title, FormField *fields, int field_count, const char *submit_label);