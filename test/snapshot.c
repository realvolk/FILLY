#include "core/render.h"
#include "core/arena.h"
#include "core/theme.h"
#include "backend/gcore/renderer.h"
#include "backend/terminal/renderer.h"
#include "backend/headless/headless.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: snapshot <reference> [--generate] [--mode pixel|ansi]\n");
        return 1;
    }

    bool generate = false;
    const char *mode = "pixel";
    const char *ref_path = argv[1];

    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--generate")) generate = true;
        else if (!strcmp(argv[i], "--mode") && i+1 < argc) mode = argv[++i];
    }

    if (strcmp(mode, "ansi") == 0) {
        RenderTree tree;
        memset(&tree, 0, sizeof(tree));
        tree.type = RNODE_CONTAINER;
        tree.rect = rect_new(0, 0, 80, 24);
        tree.resolved_style = widgetstyle_default();
        tree.container.border = BORDER_SINGLE;
        tree.container.padding = edgeinsets_zero();

        RenderTree *children = calloc(2, sizeof(RenderTree));
        children[0].type = RNODE_TEXT;
        children[0].rect = rect_new(1, 0, 78, 1);
        children[0].text.content = strdup("FILLY ANSI Snapshot");
        children[0].text.align = ALIGN_CENTER;
        children[0].resolved_style = widgetstyle_default();
        children[0].style_class = "text";
        children[0].state = "title";

        children[1].type = RNODE_TEXT;
        children[1].rect = rect_new(1, 2, 78, 1);
        children[1].text.content = strdup("Terminal output test");
        children[1].text.align = ALIGN_CENTER;
        children[1].resolved_style = widgetstyle_default();
        children[1].style_class = "text";

        tree.container.children = children;
        tree.container.child_count = 2;

        char buf[65536];
        render_tree_to_buf(&tree, 0, 0, 80, 24, buf, sizeof(buf));

        if (generate) {
            FILE *f = fopen(ref_path, "w");
            if (!f) { fprintf(stderr, "Cannot open %s\n", ref_path); return 1; }
            fwrite(buf, 1, strlen(buf), f);
            fclose(f);
            printf("ANSI reference written to %s\n", ref_path);
        } else {
            FILE *f = fopen(ref_path, "r");
            if (!f) { fprintf(stderr, "Cannot open %s\n", ref_path); return 1; }
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            rewind(f);
            char *ref = malloc(sz + 1);
            fread(ref, 1, sz, f);
            ref[sz] = '\0';
            fclose(f);
            if (strcmp(buf, ref) == 0) {
                printf("PASS: ANSI snapshot matches reference\n");
            } else {
                printf("FAIL: ANSI snapshot differs from reference\n");
                free(ref);
                free(children[0].text.content);
                free(children[1].text.content);
                free(children);
                return 1;
            }
            free(ref);
        }
        free(children[0].text.content);
        free(children[1].text.content);
        free(children);
        return 0;
    }

    gcore_init_font(NULL, 14);
    Arena *arena = arena_new(1024*1024);
    Theme *theme = theme_default();

    RenderTree tree;
    memset(&tree, 0, sizeof(tree));
    tree.type = RNODE_CONTAINER;
    tree.rect = rect_new(0, 0, 640, 480);
    tree.resolved_style = widgetstyle_default();
    tree.resolved_style.bg_color = gcore_rgba(26, 26, 46, 255);

    RenderTree *children = calloc(4, sizeof(RenderTree));

    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(10, 10, 620, 30);
    children[0].text.content = strdup("FILLY Snapshot Test");
    children[0].resolved_style = widgetstyle_default();
    children[0].resolved_style.font_size = 24;
    children[0].resolved_style.fg_color = gcore_rgba(233, 69, 96, 255);

    children[1].type = RNODE_LIST;
    children[1].rect = rect_new(10, 50, 300, 200);
    children[1].resolved_style = widgetstyle_default();
    children[1].list.item_count = 3;
    children[1].list.items = malloc(3 * sizeof(ListItem));
    children[1].list.items[0].label = strdup("Item Alpha");
    children[1].list.items[1].label = strdup("Item Beta");
    children[1].list.items[2].label = strdup("Item Gamma");
    children[1].list.selected = 1;

    children[2].type = RNODE_GAUGE;
    children[2].rect = rect_new(330, 50, 300, 60);
    children[2].gauge.percent = 75;
    children[2].gauge.label = strdup("Progress");
    children[2].resolved_style = widgetstyle_default();

    children[3].type = RNODE_TOGGLE;
    children[3].rect = rect_new(330, 130, 300, 30);
    children[3].toggle.label = strdup("Enable Feature");
    children[3].toggle.value = true;
    children[3].resolved_style = widgetstyle_default();

    tree.container.children = children;
    tree.container.child_count = 4;

    PixelBuffer pb;
    pb.width = 640; pb.height = 480; pb.stride = 640;
    pb.mouse_x = 0; pb.mouse_y = 0;
    pb.pixels = calloc(pb.width * pb.height, sizeof(uint32_t));

    gcore_render_tree_to_pixels(&tree, &pb, theme, arena);

    if (generate) {
        FILE *f = fopen(ref_path, "wb");
        fwrite(pb.pixels, sizeof(uint32_t), pb.width * pb.height, f);
        fclose(f);
        printf("Reference snapshot written to %s\n", ref_path);
    } else {
        FILE *f = fopen(ref_path, "rb");
        if (!f) { fprintf(stderr, "Cannot open reference file\n"); return 1; }
        uint32_t *ref = malloc(pb.width * pb.height * sizeof(uint32_t));
        fread(ref, sizeof(uint32_t), pb.width * pb.height, f);
        fclose(f);
        int mismatches = 0;
        for (int i = 0; i < pb.width * pb.height; i++) {
            if (pb.pixels[i] != ref[i]) mismatches++;
        }
        if (mismatches == 0) {
            printf("PASS: Snapshot matches reference\n");
        } else {
            printf("FAIL: Snapshot differs from reference (%d mismatched pixels)\n", mismatches);
            free(ref);
            free(pb.pixels);
            for (int i = 0; i < 4; i++) free(children[i].text.content);
            for (int i = 0; i < 3; i++) free(children[1].list.items[i].label);
            free(children[1].list.items);
            free(children[2].gauge.label);
            free(children[3].toggle.label);
            free(children);
            arena_free(arena);
            theme_free(theme);
            gcore_shutdown_font();
            return 1;
        }
        free(ref);
    }

    free(pb.pixels);
    free(children[0].text.content);
    free(children[1].list.items[0].label);
    free(children[1].list.items[1].label);
    free(children[1].list.items[2].label);
    free(children[1].list.items);
    free(children[2].gauge.label);
    free(children[3].toggle.label);
    free(children);
    arena_free(arena);
    theme_free(theme);
    gcore_shutdown_font();
    return 0;
}