#include "core/render.h"
#include "core/arena.h"
#include "core/theme.h"
#include "backend/gcore/renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    gcore_init_font(NULL, 14);
    bool ok = gcore_init_font(NULL, 14);
    printf("font loaded: %d\n", ok);
    Arena *arena = arena_new(1024 * 1024);
    Theme *theme = theme_default();

    RenderTree tree;
    memset(&tree, 0, sizeof(tree));
    tree.type = RNODE_CONTAINER;
    tree.rect = rect_new(0, 0, 640, 480);
    tree.resolved_style = widgetstyle_default();
    tree.resolved_style.bg_color = gcore_rgba(26, 26, 46, 255);

    RenderTree *children = calloc(2, sizeof(RenderTree));

    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(20, 20, 600, 40);
    children[0].text.content = strdup("FILLY Pixel Renderer Test");
    children[0].resolved_style = widgetstyle_default();
    children[0].resolved_style.font_size = 24;
    children[0].resolved_style.fg_color = gcore_rgba(233, 69, 96, 255);

    children[1].type = RNODE_LIST;
    children[1].rect = rect_new(50, 80, 540, 300);
    children[1].list.item_count = 5;
    children[1].list.items = malloc(5 * sizeof(ListItem));
    children[1].list.items[0] = listitem_new("Option Alpha");
    children[1].list.items[1] = listitem_new("Option Beta");
    children[1].list.items[2] = listitem_new("Option Gamma");
    children[1].list.items[3] = listitem_new("Option Delta");
    children[1].list.items[4] = listitem_new("Option Epsilon");
    children[1].list.selected = 2;
    children[1].resolved_style = widgetstyle_default();

    tree.container.children = children;
    tree.container.child_count = 2;

    PixelBuffer pb;
    pb.width = 640; pb.height = 480; pb.stride = 640;
    pb.pixels = calloc(pb.width * pb.height, sizeof(uint32_t));

    gcore_render_tree_to_pixels(&tree, &pb, theme, arena);

    FILE *f = fopen("pixel_test.ppm", "wb");
    fprintf(f, "P6\n%d %d\n255\n", pb.width, pb.height);
    for (int i = 0; i < pb.width * pb.height; i++) {
        uint32_t p = pb.pixels[i];
        uint8_t r = (p >> 16) & 0xFF;
        uint8_t g = (p >> 8) & 0xFF;
        uint8_t b = p & 0xFF;
        fwrite(&r, 1, 1, f);
        fwrite(&g, 1, 1, f);
        fwrite(&b, 1, 1, f);
    }
    fclose(f);
    printf("Wrote pixel_test.ppm\n");

    free(pb.pixels);
    free(children[0].text.content);
    for (int i = 0; i < 5; i++) free(children[1].list.items[i].label);
    free(children[1].list.items);
    free(children);
    arena_free(arena);
    theme_free(theme);
    gcore_shutdown_font();
    return 0;
}