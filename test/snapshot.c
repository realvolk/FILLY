#include "core/render.h"
#include "core/arena.h"
#include "core/theme.h"
#include "backend/gcore/renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: snapshot <reference.rgba or --generate>\n");
        return 1;
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
    children[1].list.items[0] = listitem_new("Item Alpha");
    children[1].list.items[1] = listitem_new("Item Beta");
    children[1].list.items[2] = listitem_new("Item Gamma");
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
    pb.pixels = calloc(pb.width * pb.height, sizeof(uint32_t));

    gcore_render_tree_to_pixels(&tree, &pb, theme, arena);

    if (strcmp(argv[1], "--generate") == 0) {
        FILE *f = fopen("snapshot_ref.rgba", "wb");
        fwrite(pb.pixels, sizeof(uint32_t), pb.width * pb.height, f);
        fclose(f);
        printf("Reference snapshot written to snapshot_ref.rgba\n");
    } else {
        FILE *f = fopen(argv[1], "rb");
        if (!f) { fprintf(stderr, "Cannot open reference file\n"); return 1; }
        uint32_t *ref = malloc(pb.width * pb.height * sizeof(uint32_t));
        fread(ref, sizeof(uint32_t), pb.width * pb.height, f);
        fclose(f);
        if (memcmp(pb.pixels, ref, pb.width * pb.height * sizeof(uint32_t)) == 0) {
            printf("PASS: Snapshot matches reference\n");
        } else {
            printf("FAIL: Snapshot differs from reference\n");
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