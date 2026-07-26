#include "i18n.h"
#include <stdlib.h>
#include <string.h>

void i18n_init(void) {
    setlocale(LC_ALL, "");
    bindtextdomain("filly", "/usr/share/locale");
    textdomain("filly");
}

bool i18n_is_rtl(const char *lang) {
    if (!lang) return false;
    return !strncmp(lang, "ar", 2) || !strncmp(lang, "he", 2) ||
           !strncmp(lang, "fa", 2) || !strncmp(lang, "ur", 2);
}

const char *i18n_get_language(void) {
    const char *lang = getenv("LANG");
    if (!lang) lang = getenv("LANGUAGE");
    return lang ? lang : "en";
}