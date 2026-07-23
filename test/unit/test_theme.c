#include "core/theme.h"
#include "core/render.h"
#include <stdio.h>
#include <assert.h>

int main(void) {
    printf("=== Theme Tests ===\n");

    /* default theme */
    Theme *t = theme_default();
    assert(t != NULL);
    WidgetStyle s = theme_resolve(t, "text", NULL, "normal");
    assert(s.font_size == 14);
    assert(s.fg_color != 0);

    /* forge theme */
    Theme *forge = theme_load("themes/forge.json");
    if (forge) {
        WidgetStyle title = theme_resolve(forge, "text", NULL, "title");
        assert(title.font_weight == 700);
        WidgetStyle list = theme_resolve(forge, "list", NULL, "normal");
        assert(list.bg_color != 0);
        WidgetStyle sel = theme_resolve(forge, "list", "item", "selected");
        assert(sel.accent_color != 0);
        theme_free(forge);
    } else {
        printf("SKIP: forge.json not found\n");
    }

    /* user override */
    Theme *base = theme_default();
    theme_merge_user_override(base);
    assert(base != NULL);

    theme_free(base);
    theme_free(t);
    printf("PASS: All theme tests\n");
    return 0;
}