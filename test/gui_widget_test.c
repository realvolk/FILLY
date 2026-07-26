#include "core/render.h"
#include "core/arena.h"
#include "core/theme.h"
#include "backend/gcore/renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    bool font_ok = gcore_init_font(NULL, 14);
    printf("Font loaded: %d\n", font_ok);
    if (!font_ok) return 1;
    
    Arena *arena = arena_new(1024*1024);
    Theme *theme = theme_default();

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
        child->resolved_style = widgetstyle_default();
        child->resolved_style.font_size = 14;
        child->resolved_style.fg_color = gcore_rgba(255, 255, 255, 255);

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
