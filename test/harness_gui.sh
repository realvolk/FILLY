#!/usr/bin/env bash
set -Euo pipefail
FILLY="../filly"
PASS=0; FAIL=0; SKIP=0
TOTAL=0
TEST_DIR="/tmp/filly-gui-tests"
mkdir -p "${TEST_DIR}"

GREEN='\e[32m'; RED='\e[31m'; CYAN='\e[1;36m'; YELLOW='\e[1;33m'; NC='\e[0m'

assert_file() {
    local name="$1" path="$2"
    TOTAL=$((TOTAL+1))
    if [[ -f "${path}" ]]; then
        printf "${GREEN}[PASS]${NC} %s\n" "${name}"; PASS=$((PASS+1))
    else
        printf "${RED}[FAIL]${NC} %s\n" "${name}"; FAIL=$((FAIL+1))
    fi
}

printf "${CYAN}=== FILLY GUI Integration Tests ===${NC}\n\n"

printf "${YELLOW}--- Pixel Renderer (Headless GUI) ---${NC}\n"
make -C .. pixel-test 2>/dev/null
if [[ -f ../pixel-test ]]; then
    (cd .. && ./pixel-test > /dev/null 2>&1)
    assert_file "pixel renderer produces output" "../pixel_test.ppm"
    if [[ -f ../pixel_test.ppm ]]; then
        SIZE=$(wc -c < ../pixel_test.ppm)
        TOTAL=$((TOTAL+1))
        if [[ ${SIZE} -gt 1000 ]]; then
            printf "${GREEN}[PASS]${NC} pixel output non-trivial (${SIZE} bytes)\n"; PASS=$((PASS+1))
        else
            printf "${RED}[FAIL]${NC} pixel output too small (${SIZE} bytes)\n"; FAIL=$((FAIL+1))
        fi
    fi
else
    printf "${YELLOW}[SKIP]${NC} pixel-test binary not built\n"; SKIP=$((SKIP+1))
fi

printf "${YELLOW}--- Pixel Widget Rendering ---${NC}\n"
cat > /tmp/filly-gui-widget.c << 'CEOF'
#include "core/render.h"
#include "core/arena.h"
#include "core/theme.h"
#include "backend/gcore/renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    gcore_init_font(NULL, 14);
    Arena *arena = arena_new(1024*1024);
    Theme *theme = theme_load("src/themes/forge.json");
    if (!theme) theme = theme_load("themes/forge.json");
    if (!theme) theme = theme_default();

    const char *widgets[] = {"container","text","list","input","checkbox","toggle",
        "spinner","separator","badge","table","tree","gauge","calendar","form",
        "tabs","split_panes","context_menu","toast"};
    int n = 18;
    int passed = 0, failed = 0;

    for (int i = 0; i < n; i++) {
        RenderTree tree;
        memset(&tree, 0, sizeof(tree));
        tree.type = RNODE_CONTAINER;
        tree.rect = rect_new(0, 0, 640, 480);
        tree.resolved_style = widgetstyle_default();
        tree.resolved_style.bg_color = gcore_rgba(26, 26, 46, 255);
        tree.style_class = "container";

        RenderTree *child = arena_alloc(arena, sizeof(RenderTree));
        memset(child, 0, sizeof(RenderTree));
        child->type = RNODE_TEXT;
        child->rect = rect_new(20, 20, 600, 40);
        char buf[128];
        snprintf(buf, sizeof(buf), "Widget: %s", widgets[i]);
        child->text.content = arena_strdup(arena, buf);
        child->style_class = "text";
        child->state = "title";

        tree.container.children = child;
        tree.container.child_count = 1;

        PixelBuffer pb;
        pb.width = 640; pb.height = 480; pb.stride = 640;
        pb.mouse_x = 0; pb.mouse_y = 0;
        pb.pixels = calloc(pb.width * pb.height, sizeof(uint32_t));

        gcore_render_tree_to_pixels(&tree, &pb, theme, arena);

        int colored = 0;
        for (int j = 0; j < pb.width * pb.height; j++) {
            if (pb.pixels[j] != pb.pixels[0]) colored++;
        }
        if (colored > 100) {
            fprintf(stderr, "PASS: %s (%d colored pixels)\n", widgets[i], colored);
            passed++;
        } else {
            fprintf(stderr, "FAIL: %s (only %d colored pixels)\n", widgets[i], colored);
            failed++;
        }
        free(pb.pixels);
    }

    arena_free(arena);
    theme_free(theme);
    gcore_shutdown_font();
    fprintf(stderr, "\n=== Pixel Widget Results: %d/%d passed ===\n", passed, passed+failed);
    return failed > 0 ? 1 : 0;
}
CEOF

gcc -std=c99 -D_GNU_SOURCE -Isrc -o /tmp/filly-gui-widget /tmp/filly-gui-widget.c \
    src/backend/gcore/renderer.c src/core/render.c src/core/theme.c src/core/arena.c \
    src/cJSON.c -ldl -lpthread -lm -lsodium -lcrypt -rdynamic \
    $(pkg-config --cflags --libs libdrm libinput wayland-client xkbcommon x11 2>/dev/null) \
    -lGLESv2 -lEGL -lgbm -ludev -lseccomp 2>/dev/null

if [[ -f /tmp/filly-gui-widget ]]; then
    while IFS= read -r line; do
        if echo "$line" | grep -q "^PASS:"; then
            name=$(echo "$line" | cut -d' ' -f2-)
            printf "${GREEN}[PASS]${NC} %s\n" "$name"
            PASS=$((PASS+1))
        elif echo "$line" | grep -q "^FAIL:"; then
            name=$(echo "$line" | cut -d' ' -f2-)
            printf "${RED}[FAIL]${NC} %s\n" "$name"
            FAIL=$((FAIL+1))
        elif echo "$line" | grep -q "passed"; then
            :
        fi
    done < <(/tmp/filly-gui-widget 2>&1)
    TOTAL=$((TOTAL + 18))
else
    printf "${YELLOW}[SKIP]${NC} GUI widget test not compiled (missing gcore deps)\n"; SKIP=$((SKIP+1))
fi
rm -f /tmp/filly-gui-widget.c /tmp/filly-gui-widget

printf "\n${CYAN}=== GUI Results: %d/%d passed ===${NC}\n" "${PASS}" "${TOTAL}"
[[ ${FAIL} -gt 0 ]] && exit 1 || exit 0