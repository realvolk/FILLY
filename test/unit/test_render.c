#include "core/render.h"
#include "core/theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(name) do { tests_run++; printf("  %s... ", name); } while(0)
#define CHECK(cond) do { if (!(cond)) { printf("FAIL\n"); tests_failed++; return 1; } } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)

static int test_widgetstyle_default(void) {
    TEST("widgetstyle_default");
    WidgetStyle s = widgetstyle_default();
    CHECK(s.font_size == 14);
    CHECK(s.fg_color == rgba(0xd0, 0xd0, 0xd0, 255));
    CHECK(s.bg_color == rgba(0x1a, 0x1a, 0x2e, 255));
    PASS();
    return 0;
}

static int test_theme_resolve(void) {
    TEST("theme_resolve");
    Theme *t = theme_load("src/themes/forge.json");
    if (!t) t = theme_load("themes/forge.json");
    CHECK(t != NULL);
    WidgetStyle s = theme_resolve(t, "text", NULL, "title");
    CHECK(s.font_size > 0);
    theme_free(t);
    PASS();
    return 0;
}

static int test_rgba_macro(void) {
    TEST("rgba macro");
    uint32_t c = rgba(255, 0, 0, 255);
    CHECK(c == 0xFFFF0000);
    PASS();
    return 0;
}

int main(void) {
    printf("=== FILLY Unit Tests ===\n\n");
    test_widgetstyle_default();
    test_theme_resolve();
    test_rgba_macro();
    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}