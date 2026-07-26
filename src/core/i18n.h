#pragma once
#include <libintl.h>
#include <locale.h>
#include <stdbool.h>

#define _(STRING) gettext(STRING)

void i18n_init(void);
bool i18n_is_rtl(const char *lang);
const char *i18n_get_language(void);