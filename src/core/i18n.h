#pragma once
#include <stdbool.h>

#define _(STRING) gettext(STRING)

bool i18n_is_rtl(const char *lang);
void i18n_init(void);
const char *i18n_get_language(void);