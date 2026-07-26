#pragma once
#include "core/widget.h"

Widget *input_widget_new(const char *title, const char *message, const char *default_text,
                          const char *placeholder, const char *validation);
void input_widget_set_validation_script(Widget *w, const char *script);