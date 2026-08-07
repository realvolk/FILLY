#include "shaper.h"
#include <stdlib.h>
#include <string.h>

typedef struct stbtt_fontinfo stbtt_fontinfo;
typedef struct { int glyph1, glyph2, advance; } stbtt_kerningentry;

int stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data, int offset);
int stbtt_FindGlyphIndex(stbtt_fontinfo *info, int unicode_codepoint);
float stbtt_ScaleForPixelHeight(stbtt_fontinfo *info, float pixels);
void stbtt_GetFontVMetrics(stbtt_fontinfo *info, int *ascent, int *descent, int *lineGap);
void stbtt_GetGlyphHMetrics(stbtt_fontinfo *info, int glyph_index, int *advanceWidth, int *leftSideBearing);
int stbtt_GetKerningTableLength(stbtt_fontinfo *info);
int stbtt_GetKerningTable(stbtt_fontinfo *info, stbtt_kerningentry *table, int table_length);
int stbtt_GetGlyphKernAdvance(stbtt_fontinfo *info, int g1, int g2);
unsigned char *stbtt_GetGlyphBitmap(stbtt_fontinfo *info, float scale_x, float scale_y, int glyph, int *width, int *height, int *xoff, int *yoff);

typedef struct {
    const char *pattern;
    const char *replacement;
    int pattern_len;
} LigatureEntry;

static const LigatureEntry ligature_table[] = {
    {"fi", "fi", 2},
    {"fl", "fl", 2},
    {"ff", "ff", 2},
    {"ffi", "ffi", 3},
    {"ffl", "ffl", 3},
    {"ft", "ft", 2},
    {"st", "st", 2},
    {NULL, NULL, 0}
};

static bool is_ligature_start(char c) {
    return c == 'f' || c == 's' || c == 't';
}

static int try_ligature(const char *text, int pos, int *consumed) {
    for (int i = 0; ligature_table[i].pattern != NULL; i++) {
        int len = ligature_table[i].pattern_len;
        if (strncmp(text + pos, ligature_table[i].pattern, len) == 0) {
            *consumed = len;
            return i;
        }
    }
    *consumed = 1;
    return -1;
}

static int codepoint_for_ligature(int lig_index) {
    switch (lig_index) {
        case 0: return 0xFB01;
        case 1: return 0xFB02;
        case 2: return 0xFB00;
        case 3: return 0xFB03;
        case 4: return 0xFB04;
        default: return 0;
    }
}

bool shaper_init(Shaper *s, const uint8_t *font_data, int font_data_size, float pixel_height) {
    memset(s, 0, sizeof(*s));
    s->font_data = font_data;
    s->font_data_size = font_data_size;
    s->font_info = malloc(512);
    if (!s->font_info) return false;
    if (!stbtt_InitFont((stbtt_fontinfo *)s->font_info, font_data, 0)) {
        free(s->font_info);
        return false;
    }
    s->scale = stbtt_ScaleForPixelHeight((stbtt_fontinfo *)s->font_info, pixel_height);
    int kern_len = stbtt_GetKerningTableLength((stbtt_fontinfo *)s->font_info);
    if (kern_len > 0) {
        s->kern_table = malloc(kern_len * 3 * sizeof(int));
        stbtt_kerningentry *entries = malloc(kern_len * sizeof(stbtt_kerningentry));
        int count = stbtt_GetKerningTable((stbtt_fontinfo *)s->font_info, entries, kern_len);
        for (int i = 0; i < count; i++) {
            s->kern_table[i * 3] = entries[i].glyph1;
            s->kern_table[i * 3 + 1] = entries[i].glyph2;
            s->kern_table[i * 3 + 2] = entries[i].advance;
        }
        s->kern_count = count;
        free(entries);
    }
    s->kerning_loaded = true;
    return true;
}

void shaper_destroy(Shaper *s) {
    free(s->font_info);
    free(s->kern_table);
    memset(s, 0, sizeof(*s));
}

float shaper_get_kerning(Shaper *s, int codepoint_a, int codepoint_b) {
    if (!s->kerning_loaded) return 0.0f;
    int g1 = stbtt_FindGlyphIndex((stbtt_fontinfo *)s->font_info, codepoint_a);
    int g2 = stbtt_FindGlyphIndex((stbtt_fontinfo *)s->font_info, codepoint_b);
    if (g1 == 0 || g2 == 0) return 0.0f;
    for (int i = 0; i < s->kern_count; i++) {
        if (s->kern_table[i * 3] == g1 && s->kern_table[i * 3 + 1] == g2)
            return s->kern_table[i * 3 + 2] * s->scale;
    }
    int gpos_advance = stbtt_GetGlyphKernAdvance((stbtt_fontinfo *)s->font_info, g1, g2);
    return gpos_advance * s->scale;
}

ShapedText *shaper_shape(Shaper *s, const char *text, float letter_spacing) {
    if (!text || !text[0]) {
        ShapedText *st = calloc(1, sizeof(ShapedText));
        return st;
    }
    int text_len = strlen(text);
    ShapedText *st = calloc(1, sizeof(ShapedText));
    st->glyphs = malloc(text_len * sizeof(ShapedGlyph));
    st->count = 0;
    int prev_codepoint = 0;
    float pen_x = 0.0f;

    for (int i = 0; text[i]; ) {
        int consumed = 0;
        int lig_index = -1;
        int codepoint;

        if (is_ligature_start(text[i])) {
            lig_index = try_ligature(text, i, &consumed);
        }

        if (lig_index >= 0) {
            codepoint = codepoint_for_ligature(lig_index);
            i += consumed;
        } else {
            codepoint = (unsigned char)text[i];
            i++;
        }

        int glyph_index = stbtt_FindGlyphIndex((stbtt_fontinfo *)s->font_info, codepoint);
        if (glyph_index == 0) {
            codepoint = text[i - 1];
            glyph_index = stbtt_FindGlyphIndex((stbtt_fontinfo *)s->font_info, codepoint);
        }

        if (st->count > 0 && prev_codepoint != 0) {
            float kern = shaper_get_kerning(s, prev_codepoint, codepoint);
            pen_x += kern;
        }

        ShapedGlyph *sg = &st->glyphs[st->count++];
        sg->glyph_index = glyph_index;
        sg->codepoint = codepoint;
        sg->x_offset = pen_x;
        sg->y_offset = 0.0f;

        int advance, lsb;
        stbtt_GetGlyphHMetrics((stbtt_fontinfo *)s->font_info, glyph_index, &advance, &lsb);
        sg->x_advance = advance * s->scale + letter_spacing;

        pen_x += sg->x_advance;
        prev_codepoint = codepoint;
    }

    st->total_advance = pen_x;
    return st;
}

void shaped_text_free(ShapedText *st) {
    if (!st) return;
    free(st->glyphs);
    free(st);
}

bool shaper_glyph_bitmap(Shaper *s, uint32_t glyph_index, uint8_t **bitmap, int *w, int *h, int *xoff, int *yoff) {
    if (!s || glyph_index == 0) {
        *bitmap = NULL;
        *w = 0; *h = 0;
        *xoff = 0; *yoff = 0;
        return false;
    }
    *bitmap = stbtt_GetGlyphBitmap((stbtt_fontinfo *)s->font_info, s->scale, s->scale, glyph_index, w, h, xoff, yoff);
    return *bitmap != NULL;
}