#include "core/render.h"
#include "core/arena.h"
#include "core/theme.h"
#include "core/shaper.h"
#include "core/vector.h"
#include "backend/gcore/renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(n) do { tests_run++; printf("  %s... ", n); } while(0)
#define CHECK(c) do { if (!(c)) { printf("FAIL\n"); tests_failed++; return; } } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)

static int count_colored(PixelBuffer *pb) {
    int colored = 0;
    uint32_t bg = pb->pixels[0];
    for (int i = 0; i < pb->width * pb->height; i++)
        if (pb->pixels[i] != bg) colored++;
    return colored;
}

static int count_color(PixelBuffer *pb, uint32_t color) {
    int count = 0;
    for (int i = 0; i < pb->width * pb->height; i++)
        if (pb->pixels[i] == color) count++;
    return count;
}

static int has_color_in_region(PixelBuffer *pb, int x, int y, int w, int h, uint32_t color) {
    for (int row = y; row < y + h && row < pb->height; row++)
        for (int col = x; col < x + w && col < pb->width; col++)
            if (pb->pixels[row * pb->width + col] == color) return 1;
    return 0;
}

static int colors_in_region(PixelBuffer *pb, int x, int y, int w, int h) {
    uint32_t first = pb->pixels[y * pb->width + x];
    for (int row = y; row < y + h && row < pb->height; row++)
        for (int col = x; col < x + w && col < pb->width; col++)
            if (pb->pixels[row * pb->width + col] != first) return 1;
    return 0;
}

static void test_shadow_rendering(void) {
    TEST("shadow rendering produces blur outside rect");
    PixelBuffer pb;
    pb.width = 400; pb.height = 300; pb.stride = 400;
    pb.mouse_x = 0; pb.mouse_y = 0;
    pb.pixels = calloc(pb.width * pb.height, sizeof(uint32_t));
    uint32_t bg = gcore_rgba(26, 26, 46, 255);
    for (int i = 0; i < pb.width * pb.height; i++) pb.pixels[i] = bg;

    RenderTree node;
    memset(&node, 0, sizeof(node));
    node.type = RNODE_CONTAINER;
    node.rect = rect_new(100, 80, 200, 100);
    node.resolved_style = widgetstyle_default();
    node.resolved_style.bg_color = gcore_rgba(40, 40, 60, 255);
    node.resolved_style.shadows[0].offset_x = 4;
    node.resolved_style.shadows[0].offset_y = 4;
    node.resolved_style.shadows[0].blur = 8;
    node.resolved_style.shadows[0].spread = 2;
    node.resolved_style.shadows[0].color = gcore_rgba(0, 0, 0, 180);
    node.resolved_style.shadow_count = 1;
    node.dirty = true;

    Rect dirty = {0, 0, 0, 0};
    render_node(&node, 0, 0, &pb, NULL, NULL, true, dirty);

    CHECK(count_colored(&pb) > 500);
    CHECK(has_color_in_region(&pb, 90, 70, 20, 20, bg) || colors_in_region(&pb, 90, 70, 20, 20));
    CHECK(has_color_in_region(&pb, 310, 190, 20, 20, bg) || colors_in_region(&pb, 310, 190, 20, 20));
    free(pb.pixels);
    PASS();
}

static void test_gradient_rendering(void) {
    TEST("gradient produces color variation");
    PixelBuffer pb;
    pb.width = 400; pb.height = 300; pb.stride = 400;
    pb.mouse_x = 0; pb.mouse_y = 0;
    pb.pixels = calloc(pb.width * pb.height, sizeof(uint32_t));
    uint32_t bg = gcore_rgba(26, 26, 46, 255);
    for (int i = 0; i < pb.width * pb.height; i++) pb.pixels[i] = bg;

    RenderTree node;
    memset(&node, 0, sizeof(node));
    node.type = RNODE_CONTAINER;
    node.rect = rect_new(50, 50, 300, 200);
    node.resolved_style = widgetstyle_default();
    node.resolved_style.gradient.type = GRADIENT_LINEAR;
    node.resolved_style.gradient.angle = 90.0f;
    node.resolved_style.gradient.stop_count = 2;
    node.resolved_style.gradient.stops[0].offset = 0.0f;
    node.resolved_style.gradient.stops[0].color = gcore_rgba(255, 0, 0, 255);
    node.resolved_style.gradient.stops[1].offset = 1.0f;
    node.resolved_style.gradient.stops[1].color = gcore_rgba(0, 0, 255, 255);
    node.dirty = true;

    Rect dirty = {0, 0, 0, 0};
    render_node(&node, 0, 0, &pb, NULL, NULL, true, dirty);

    CHECK(count_colored(&pb) > 500);
    uint32_t top_color = pb.pixels[55 * pb.width + 200];
    uint32_t bottom_color = pb.pixels[245 * pb.width + 200];
    CHECK(top_color != bottom_color);
    free(pb.pixels);
    PASS();
}

static void test_multi_shadow(void) {
    TEST("multiple shadows render");
    PixelBuffer pb;
    pb.width = 400; pb.height = 300; pb.stride = 400;
    pb.mouse_x = 0; pb.mouse_y = 0;
    pb.pixels = calloc(pb.width * pb.height, sizeof(uint32_t));
    uint32_t bg = gcore_rgba(26, 26, 46, 255);
    for (int i = 0; i < pb.width * pb.height; i++) pb.pixels[i] = bg;

    RenderTree node;
    memset(&node, 0, sizeof(node));
    node.type = RNODE_CONTAINER;
    node.rect = rect_new(100, 100, 200, 80);
    node.resolved_style = widgetstyle_default();
    node.resolved_style.bg_color = gcore_rgba(60, 60, 80, 255);
    node.resolved_style.shadows[0].offset_x = 3;
    node.resolved_style.shadows[0].offset_y = 3;
    node.resolved_style.shadows[0].blur = 6;
    node.resolved_style.shadows[0].color = gcore_rgba(255, 0, 0, 120);
    node.resolved_style.shadows[1].offset_x = -3;
    node.resolved_style.shadows[1].offset_y = 6;
    node.resolved_style.shadows[1].blur = 10;
    node.resolved_style.shadows[1].color = gcore_rgba(0, 0, 255, 100);
    node.resolved_style.shadow_count = 2;
    node.dirty = true;

    Rect dirty = {0, 0, 0, 0};
    render_node(&node, 0, 0, &pb, NULL, NULL, true, dirty);

    CHECK(count_colored(&pb) > 300);
    free(pb.pixels);
    PASS();
}

static void test_per_side_borders(void) {
    TEST("per-side borders render independently");
    PixelBuffer pb;
    pb.width = 200; pb.height = 200; pb.stride = 200;
    pb.mouse_x = 0; pb.mouse_y = 0;
    pb.pixels = calloc(pb.width * pb.height, sizeof(uint32_t));
    uint32_t bg = gcore_rgba(26, 26, 46, 255);
    for (int i = 0; i < pb.width * pb.height; i++) pb.pixels[i] = bg;

    RenderTree node;
    memset(&node, 0, sizeof(node));
    node.type = RNODE_CONTAINER;
    node.rect = rect_new(50, 50, 100, 100);
    node.resolved_style = widgetstyle_default();
    node.resolved_style.bg_color = gcore_rgba(40, 40, 60, 255);
    node.resolved_style.border_top_width = 4;
    node.resolved_style.border_top_color = gcore_rgba(255, 0, 0, 255);
    node.resolved_style.border_bottom_width = 0;
    node.resolved_style.border_left_width = 2;
    node.resolved_style.border_left_color = gcore_rgba(0, 255, 0, 255);
    node.resolved_style.border_right_width = 6;
    node.resolved_style.border_right_color = gcore_rgba(0, 0, 255, 255);
    node.dirty = true;

    Rect dirty = {0, 0, 0, 0};
    render_node(&node, 0, 0, &pb, NULL, NULL, true, dirty);

    CHECK(has_color_in_region(&pb, 50, 50, 100, 4, gcore_rgba(255, 0, 0, 255)));
    CHECK(has_color_in_region(&pb, 50, 50, 2, 100, gcore_rgba(0, 255, 0, 255)));
    CHECK(has_color_in_region(&pb, 148, 50, 6, 100, gcore_rgba(0, 0, 255, 255)));
    free(pb.pixels);
    PASS();
}

static void test_vector_rendering(void) {
    TEST("vector path rasterizes to pixels");
    PixelBuffer pb;
    pb.width = 300; pb.height = 300; pb.stride = 300;
    pb.mouse_x = 0; pb.mouse_y = 0;
    pb.pixels = calloc(pb.width * pb.height, sizeof(uint32_t));
    uint32_t bg = gcore_rgba(26, 26, 46, 255);
    for (int i = 0; i < pb.width * pb.height; i++) pb.pixels[i] = bg;

    VectorPath *vp = vector_parse_svg_path("M50,50 L250,50 L250,250 L50,250 Z", 1.0f);
    CHECK(vp != NULL);
    CHECK(vp->vertex_count >= 4);
    CHECK(vp->contour_count >= 1);

    int vx, vy, vw, vh;
    vector_get_bounds(vp, &vx, &vy, &vw, &vh, 1.0f);
    CHECK(vw > 0 && vh > 0);

    vector_rasterize_to_buffer(vp, (uint8_t *)pb.pixels, pb.width, pb.height, pb.width, 1.0f, -vx + 10, -vy + 10);
    CHECK(count_colored(&pb) > 100);
    vector_path_free(vp);
    free(pb.pixels);
    PASS();
}

static void test_vector_cubic_bezier(void) {
    TEST("vector cubic bezier renders");
    PixelBuffer pb;
    pb.width = 300; pb.height = 300; pb.stride = 300;
    pb.mouse_x = 0; pb.mouse_y = 0;
    pb.pixels = calloc(pb.width * pb.height, sizeof(uint32_t));
    uint32_t bg = gcore_rgba(26, 26, 46, 255);
    for (int i = 0; i < pb.width * pb.height; i++) pb.pixels[i] = bg;

    VectorPath *vp = vector_parse_svg_path("M50,150 C50,50 250,50 250,150 S50,250 50,150 Z", 1.0f);
    CHECK(vp != NULL);
    CHECK(vp->vertex_count > 4);

    vector_rasterize_to_buffer(vp, (uint8_t *)pb.pixels, pb.width, pb.height, pb.width, 1.0f, 0, 0);
    CHECK(count_colored(&pb) > 50);
    vector_path_free(vp);
    free(pb.pixels);
    PASS();
}

static void test_kerning_output(void) {
    TEST("shaper produces kerned glyph positions");
    bool font_ok = gcore_init_font(NULL, 14);
    CHECK(font_ok);

    Shaper shaper;
    bool ok = shaper_init(&shaper, font_data, font_data_size, 24.0f);
    CHECK(ok);

    ShapedText *st1 = shaper_shape(&shaper, "AV", 0.0f);
    CHECK(st1 != NULL);
    CHECK(st1->count >= 2);
    float av_advance = st1->total_advance;
    shaped_text_free(st1);

    ShapedText *st2 = shaper_shape(&shaper, "AA", 0.0f);
    CHECK(st2 != NULL);
    CHECK(st2->count >= 2);
    float aa_advance = st2->total_advance;
    shaped_text_free(st2);

    shaper_destroy(&shaper);
    gcore_shutdown_font();
    PASS();
}

static void test_ligature_substitution(void) {
    TEST("shaper substitutes ligatures");
    bool font_ok = gcore_init_font(NULL, 14);
    CHECK(font_ok);

    Shaper shaper;
    bool ok = shaper_init(&shaper, font_data, font_data_size, 24.0f);
    CHECK(ok);

    ShapedText *st_fi = shaper_shape(&shaper, "fi", 0.0f);
    CHECK(st_fi != NULL);
    ShapedText *st_fl = shaper_shape(&shaper, "fl", 0.0f);
    CHECK(st_fl != NULL);
    ShapedText *st_ffi = shaper_shape(&shaper, "ffi", 0.0f);
    CHECK(st_ffi != NULL);

    shaped_text_free(st_fi);
    shaped_text_free(st_fl);
    shaped_text_free(st_ffi);
    shaper_destroy(&shaper);
    gcore_shutdown_font();
    PASS();
}

static void test_vector_relative_commands(void) {
    TEST("vector relative commands parse correctly");
    VectorPath *vp = vector_parse_svg_path("m10,20 l30,40 l-10,-10 z", 1.0f);
    CHECK(vp != NULL);
    CHECK(vp->vertex_count >= 3);
    vector_path_free(vp);
    PASS();
}

static void test_vector_smooth_commands(void) {
    TEST("vector smooth curve commands parse");
    VectorPath *vp = vector_parse_svg_path("M50,100 Q100,50 150,100 T250,100", 1.0f);
    CHECK(vp != NULL);
    CHECK(vp->vertex_count > 3);
    vector_path_free(vp);
    PASS();
}

static void test_gradient_radial(void) {
    TEST("radial gradient produces circular pattern");
    PixelBuffer pb;
    pb.width = 200; pb.height = 200; pb.stride = 200;
    pb.mouse_x = 0; pb.mouse_y = 0;
    pb.pixels = calloc(pb.width * pb.height, sizeof(uint32_t));

    RenderTree node;
    memset(&node, 0, sizeof(node));
    node.type = RNODE_CONTAINER;
    node.rect = rect_new(0, 0, 200, 200);
    node.resolved_style = widgetstyle_default();
    node.resolved_style.gradient.type = GRADIENT_RADIAL;
    node.resolved_style.gradient.center_x = 0.5f;
    node.resolved_style.gradient.center_y = 0.5f;
    node.resolved_style.gradient.stop_count = 2;
    node.resolved_style.gradient.stops[0].offset = 0.0f;
    node.resolved_style.gradient.stops[0].color = gcore_rgba(255, 255, 255, 255);
    node.resolved_style.gradient.stops[1].offset = 1.0f;
    node.resolved_style.gradient.stops[1].color = gcore_rgba(0, 0, 0, 255);
    node.dirty = true;

    Rect dirty = {0, 0, 0, 0};
    render_node(&node, 0, 0, &pb, NULL, NULL, true, dirty);

    uint32_t center = pb.pixels[100 * pb.width + 100];
    uint32_t corner = pb.pixels[5 * pb.width + 5];
    CHECK(center != corner);
    free(pb.pixels);
    PASS();
}

static void test_opacity_blending(void) {
    TEST("opacity blends with background");
    PixelBuffer pb;
    pb.width = 100; pb.height = 100; pb.stride = 100;
    pb.mouse_x = 0; pb.mouse_y = 0;
    pb.pixels = calloc(pb.width * pb.height, sizeof(uint32_t));
    uint32_t bg = gcore_rgba(255, 255, 255, 255);
    for (int i = 0; i < pb.width * pb.height; i++) pb.pixels[i] = bg;

    RenderTree node;
    memset(&node, 0, sizeof(node));
    node.type = RNODE_CONTAINER;
    node.rect = rect_new(10, 10, 80, 80);
    node.resolved_style = widgetstyle_default();
    node.resolved_style.bg_color = gcore_rgba(255, 0, 0, 255);
    node.resolved_style.opacity = 0.5f;
    node.dirty = true;

    Rect dirty = {0, 0, 0, 0};
    render_node(&node, 0, 0, &pb, NULL, NULL, true, dirty);

    uint32_t blended = pb.pixels[50 * pb.width + 50];
    uint8_t r = (blended >> 16) & 0xFF;
    CHECK(r > 128 && r < 255);
    free(pb.pixels);
    PASS();
}

int main(void) {
    printf("=== FILLY v2.0 Pixel Tests ===\n\n");

    bool font_ok = gcore_init_font(NULL, 14);
    if (!font_ok) {
        printf("SKIP: font not found, skipping font-dependent tests\n");
        tests_run += 3;
        printf("  kerning_output... SKIP (no font)\n");
        printf("  ligature_substitution... SKIP (no font)\n");
    } else {
        gcore_shutdown_font();
    }

    test_shadow_rendering();
    test_multi_shadow();
    test_gradient_rendering();
    test_gradient_radial();
    test_per_side_borders();
    test_opacity_blending();
    test_vector_rendering();
    test_vector_cubic_bezier();
    test_vector_relative_commands();
    test_vector_smooth_commands();

    if (font_ok) {
        test_kerning_output();
        test_ligature_substitution();
    }

    printf("\n=== v2.0 Pixel Results: %d/%d passed ===\n", tests_passed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}