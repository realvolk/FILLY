#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t glyph_index;
    int codepoint;
    float x_offset;
    float y_offset;
    float x_advance;
} ShapedGlyph;

typedef struct {
    ShapedGlyph *glyphs;
    int count;
    float total_advance;
} ShapedText;

typedef struct {
    const uint8_t *font_data;
    int font_data_size;
    void *font_info;
    float scale;
    bool kerning_loaded;
    int *kern_table;
    int kern_count;
} Shaper;

bool shaper_init(Shaper *s, const uint8_t *font_data, int font_data_size, float pixel_height);
void shaper_destroy(Shaper *s);
ShapedText *shaper_shape(Shaper *s, const char *text, float letter_spacing);
void shaped_text_free(ShapedText *st);
bool shaper_glyph_bitmap(Shaper *s, uint32_t glyph_index, uint8_t **bitmap, int *w, int *h, int *xoff, int *yoff);
float shaper_get_kerning(Shaper *s, int codepoint_a, int codepoint_b);