/* stb_truetype.h — Minimal TTF glyph rasterizer for FILLY
 * -----------------------------------------------------------------------------
 * Stripped from stb_truetype v1.26 by Sean Barrett (public domain / MIT).
 * https://github.com/nothings/stb
 *
 * FILLY only needs: InitFont, ScaleForPixelHeight, GetFontVMetrics,
 * GetCodepointHMetrics, GetCodepointBitmap, FreeBitmap.
 * Only TrueType (glyf/loca) with cmap format 4. No CFF, no SDF, no baking.
 * -----------------------------------------------------------------------------
 */
#ifndef FILLY_TRUETYPE_H
#define FILLY_TRUETYPE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char  stbtt_uint8;
typedef signed   char  stbtt_int8;
typedef unsigned short stbtt_uint16;
typedef signed   short stbtt_int16;
typedef unsigned int   stbtt_uint32;
typedef signed   int   stbtt_int32;

typedef struct {
    unsigned char *data;
    int cursor;
    int size;
} stbtt__buf;

typedef struct {
    void *userdata;
    unsigned char *data;
    int fontstart;
    int numGlyphs;
    int loca, head, glyf, hhea, hmtx;
    int index_map;
    int indexToLocFormat;
} stbtt_fontinfo;

int stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data, int offset);
int stbtt_FindGlyphIndex(const stbtt_fontinfo *info, int unicode_codepoint);
float stbtt_ScaleForPixelHeight(const stbtt_fontinfo *info, float pixels);
void stbtt_GetFontVMetrics(const stbtt_fontinfo *info, int *ascent, int *descent, int *lineGap);
void stbtt_GetCodepointHMetrics(const stbtt_fontinfo *info, int codepoint, int *advanceWidth, int *leftSideBearing);
unsigned char *stbtt_GetCodepointBitmap(const stbtt_fontinfo *info, float scale_x, float scale_y, int codepoint, int *width, int *height, int *xoff, int *yoff);
void stbtt_FreeBitmap(unsigned char *bitmap, void *userdata);

#ifdef __cplusplus
}
#endif

#endif /* FILLY_TRUETYPE_H */

#ifdef STB_TRUETYPE_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STBTT_malloc(x,u)  ((void)(u), malloc(x))
#define STBTT_free(x,u)    ((void)(u), free(x))
#define STBTT_memset       memset
#define STBTT_memcpy       memcpy
#define STBTT_ifloor(x)    ((int) floor(x))
#define STBTT_iceil(x)     ((int) ceil(x))
#define STBTT_sqrt(x)      sqrt(x)
#define STBTT_fabs(x)      fabs(x)
#define STBTT_assert(x)

/* ---------------------------------------------------------------------------
 * buf helpers
 * --------------------------------------------------------------------------- */
static stbtt_uint8 stbtt__buf_get8(stbtt__buf *b) {
    if (b->cursor >= b->size) return 0;
    return b->data[b->cursor++];
}
static stbtt_uint32 stbtt__buf_get(stbtt__buf *b, int n) {
    stbtt_uint32 v = 0;
    for (int i = 0; i < n; i++) v = (v << 8) | stbtt__buf_get8(b);
    return v;
}
#define stbtt__buf_get16(b)  stbtt__buf_get((b), 2)
#define stbtt__buf_get32(b)  stbtt__buf_get((b), 4)

/* ---------------------------------------------------------------------------
 * TTF file accessors
 * --------------------------------------------------------------------------- */
#define ttBYTE(p)     (* (stbtt_uint8 *) (p))
static stbtt_uint16 ttUSHORT(stbtt_uint8 *p) { return p[0]*256 + p[1]; }
static stbtt_int16  ttSHORT(stbtt_uint8 *p)  { return p[0]*256 + p[1]; }
static stbtt_uint32 ttULONG(stbtt_uint8 *p)  { return (p[0]<<24) | (p[1]<<16) | (p[2]<<8) | p[3]; }
#define stbtt_tag4(p,c0,c1,c2,c3) ((p)[0]==(c0) && (p)[1]==(c1) && (p)[2]==(c2) && (p)[3]==(c3))
#define stbtt_tag(p,str) stbtt_tag4(p,str[0],str[1],str[2],str[3])

static int stbtt__isfont(stbtt_uint8 *font) {
    if (stbtt_tag4(font,'1',0,0,0)) return 1;
    if (stbtt_tag(font,"true"))    return 1;
    if (stbtt_tag4(font,0,1,0,0)) return 1;
    return 0;
}

static stbtt_uint32 stbtt__find_table(stbtt_uint8 *data, stbtt_uint32 fontstart, const char *tag) {
    stbtt_int32 num_tables = ttUSHORT(data + fontstart + 4);
    stbtt_uint32 tabledir = fontstart + 12;
    for (int i = 0; i < num_tables; i++) {
        stbtt_uint32 loc = tabledir + 16*i;
        if (stbtt_tag(data+loc, tag)) return ttULONG(data+loc+8);
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * InitFont
 * --------------------------------------------------------------------------- */
int stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data, int offset) {
    info->data = (unsigned char *)data;
    info->fontstart = offset;
    info->userdata = NULL;

    stbtt_uint32 cmap   = stbtt__find_table(info->data, offset, "cmap");
    info->loca  = stbtt__find_table(info->data, offset, "loca");
    info->head  = stbtt__find_table(info->data, offset, "head");
    info->glyf  = stbtt__find_table(info->data, offset, "glyf");
    info->hhea  = stbtt__find_table(info->data, offset, "hhea");
    info->hmtx  = stbtt__find_table(info->data, offset, "hmtx");

    if (!cmap || !info->head || !info->hhea || !info->hmtx || !info->glyf || !info->loca)
        return 0;

    stbtt_uint32 t = stbtt__find_table(info->data, offset, "maxp");
    info->numGlyphs = t ? ttUSHORT(info->data+t+4) : 0xffff;
    info->indexToLocFormat = ttUSHORT(info->data + info->head + 50);

    /* find cmap format 4 */
    int numTables = ttUSHORT(info->data + cmap + 2);
    info->index_map = 0;
    for (int i = 0; i < numTables; i++) {
        stbtt_uint32 er = cmap + 4 + 8*i;
        if (ttUSHORT(info->data+er) == 3 && (ttUSHORT(info->data+er+2) == 1 || ttUSHORT(info->data+er+2) == 10)) {
            info->index_map = cmap + ttULONG(info->data+er+4);
            break;
        }
        if (ttUSHORT(info->data+er) == 0) {
            info->index_map = cmap + ttULONG(info->data+er+4);
            break;
        }
    }
    return info->index_map != 0;
}

/* ---------------------------------------------------------------------------
 * FindGlyphIndex — cmap format 4 only
 * --------------------------------------------------------------------------- */
int stbtt_FindGlyphIndex(const stbtt_fontinfo *info, int codepoint) {
    stbtt_uint8 *data = info->data;
    stbtt_uint32 im = info->index_map;

    stbtt_uint16 format = ttUSHORT(data + im);
    if (format == 0) {
        int bytes = ttUSHORT(data + im + 2);
        if (codepoint < bytes-6) return ttBYTE(data + im + 6 + codepoint);
        return 0;
    }
    if (format == 4) {
        stbtt_uint16 segcount = ttUSHORT(data+im+6) >> 1;
        stbtt_uint32 endCount = im + 14;
        stbtt_uint32 search = endCount;
        if (codepoint > 0xffff) return 0;
        if (codepoint >= ttUSHORT(data + search + (segcount >> 1)*2))
            search += (segcount >> 1)*2;
        search -= 2;
        {
            stbtt_uint16 entrySelector = ttUSHORT(data+im+10);
            stbtt_uint16 searchRange = ttUSHORT(data+im+8) >> 1;
            while (entrySelector) {
                searchRange >>= 1;
                if (codepoint > ttUSHORT(data + search + searchRange*2))
                    search += searchRange*2;
                entrySelector--;
            }
        }
        search += 2;
        {
            stbtt_uint16 item = (stbtt_uint16)((search - endCount) >> 1);
            stbtt_uint16 start = ttUSHORT(data + im + 14 + segcount*2 + 2 + 2*item);
            stbtt_uint16 last  = ttUSHORT(data + endCount + 2*item);
            if (codepoint < start || codepoint > last) return 0;
            stbtt_uint16 offset = ttUSHORT(data + im + 14 + segcount*6 + 2 + 2*item);
            if (offset == 0) return (stbtt_uint16)(codepoint + ttSHORT(data + im + 14 + segcount*4 + 2 + 2*item));
            return ttUSHORT(data + offset + (codepoint-start)*2 + im + 14 + segcount*6 + 2 + 2*item);
        }
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * Metrics
 * --------------------------------------------------------------------------- */
float stbtt_ScaleForPixelHeight(const stbtt_fontinfo *info, float pixels) {
    int fh = ttSHORT(info->data + info->hhea + 4) - ttSHORT(info->data + info->hhea + 6);
    return pixels / (float)fh;
}

void stbtt_GetFontVMetrics(const stbtt_fontinfo *info, int *ascent, int *descent, int *lineGap) {
    if (ascent)  *ascent  = ttSHORT(info->data + info->hhea + 4);
    if (descent) *descent = ttSHORT(info->data + info->hhea + 6);
    if (lineGap) *lineGap = ttSHORT(info->data + info->hhea + 8);
}

void stbtt_GetCodepointHMetrics(const stbtt_fontinfo *info, int codepoint, int *advanceWidth, int *leftSideBearing) {
    int g = stbtt_FindGlyphIndex(info, codepoint);
    stbtt_uint16 numOfLongHorMetrics = ttUSHORT(info->data + info->hhea + 34);
    if (g < numOfLongHorMetrics) {
        if (advanceWidth)    *advanceWidth    = ttSHORT(info->data + info->hmtx + 4*g);
        if (leftSideBearing) *leftSideBearing = ttSHORT(info->data + info->hmtx + 4*g + 2);
    } else {
        if (advanceWidth)    *advanceWidth    = ttSHORT(info->data + info->hmtx + 4*(numOfLongHorMetrics-1));
        if (leftSideBearing) *leftSideBearing = ttSHORT(info->data + info->hmtx + 4*numOfLongHorMetrics + 2*(g - numOfLongHorMetrics));
    }
}

static int stbtt__GetGlyfOffset(const stbtt_fontinfo *info, int glyph_index) {
    if (glyph_index >= info->numGlyphs) return -1;
    if (info->indexToLocFormat >= 2)    return -1;
    if (info->indexToLocFormat == 0) {
        int g1 = info->glyf + ttUSHORT(info->data + info->loca + glyph_index*2)*2;
        int g2 = info->glyf + ttUSHORT(info->data + info->loca + glyph_index*2 + 2)*2;
        return g1 == g2 ? -1 : g1;
    } else {
        int g1 = info->glyf + ttULONG(info->data + info->loca + glyph_index*4);
        int g2 = info->glyf + ttULONG(info->data + info->loca + glyph_index*4 + 4);
        return g1 == g2 ? -1 : g1;
    }
}

static int stbtt_GetGlyphBox(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1) {
    int g = stbtt__GetGlyfOffset(info, glyph_index);
    if (g < 0) return 0;
    if (x0) *x0 = ttSHORT(info->data + g + 2);
    if (y0) *y0 = ttSHORT(info->data + g + 4);
    if (x1) *x1 = ttSHORT(info->data + g + 6);
    if (y1) *y1 = ttSHORT(info->data + g + 8);
    return 1;
}

/* ---------------------------------------------------------------------------
 * Glyph shape extraction (TrueType only, no compound glyphs)
 * --------------------------------------------------------------------------- */
enum { STBTT_vmove=1, STBTT_vline, STBTT_vcurve };

typedef struct {
    short x, y, cx, cy;
    unsigned char type;
} stbtt_vertex;

static void stbtt_setvertex(stbtt_vertex *v, stbtt_uint8 type, int x, int y, int cx, int cy) {
    v->type = type;
    v->x = (short)x; v->y = (short)y;
    v->cx = (short)cx; v->cy = (short)cy;
}

static int stbtt_GetGlyphShape(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **pvertices) {
    int g = stbtt__GetGlyfOffset(info, glyph_index);
    *pvertices = NULL;
    if (g < 0) return 0;

    stbtt_int16 numberOfContours = ttSHORT(info->data + g);
    if (numberOfContours <= 0) return 0; /* skip compound and empty glyphs */

    stbtt_uint8 *endPtsOfContours = info->data + g + 10;
    int ins = ttUSHORT(info->data + g + 10 + numberOfContours*2);
    stbtt_uint8 *points = info->data + g + 10 + numberOfContours*2 + 2 + ins;
    int n = 1 + ttUSHORT(endPtsOfContours + numberOfContours*2 - 2);
    int m = n + 2*numberOfContours;

    stbtt_vertex *vertices = (stbtt_vertex *)STBTT_malloc(m * sizeof(stbtt_vertex), info->userdata);
    if (!vertices) return 0;

    /* decode flags */
    int off = m - n, flagcount = 0;
    for (int i = 0; i < n; i++) {
        if (flagcount == 0) {
            stbtt_uint8 flags = *points++;
            if (flags & 8) flagcount = *points++;
            vertices[off+i].type = flags;
        } else {
            vertices[off+i].type = vertices[off+i-1].type;
            flagcount--;
        }
    }
    /* decode x */
    int x = 0;
    for (int i = 0; i < n; i++) {
        stbtt_uint8 flags = vertices[off+i].type;
        if (flags & 2) {
            stbtt_int16 dx = *points++;
            x += (flags & 16) ? dx : -dx;
        } else if (!(flags & 16)) {
            x += (stbtt_int16)(points[0]*256 + points[1]);
            points += 2;
        }
        vertices[off+i].x = (short)x;
    }
    /* decode y */
    int y = 0;
    for (int i = 0; i < n; i++) {
        stbtt_uint8 flags = vertices[off+i].type;
        if (flags & 4) {
            stbtt_int16 dy = *points++;
            y += (flags & 32) ? dy : -dy;
        } else if (!(flags & 32)) {
            y += (stbtt_int16)(points[0]*256 + points[1]);
            points += 2;
        }
        vertices[off+i].y = (short)y;
    }
    /* build contours */
    int num_vertices = 0, sx = 0, sy = 0, cx = 0, cy = 0, was_off = 0, start_off = 0;
    int j = 0, next_move = 0;
    for (int i = 0; i < n; i++) {
        stbtt_uint8 flags = vertices[off+i].type;
        x = vertices[off+i].x; y = vertices[off+i].y;
        if (next_move == i) {
            start_off = !(flags & 1);
            if (start_off) {
                if (!(vertices[off+i+1].type & 1)) {
                    sx = (x + vertices[off+i+1].x) >> 1;
                    sy = (y + vertices[off+i+1].y) >> 1;
                } else {
                    sx = vertices[off+i+1].x; sy = vertices[off+i+1].y; i++;
                }
            } else { sx = x; sy = y; }
            stbtt_setvertex(&vertices[num_vertices++], STBTT_vmove, sx, sy, 0, 0);
            was_off = 0;
            next_move = 1 + ttUSHORT(endPtsOfContours + j*2);
            j++;
        } else {
            if (!(flags & 1)) {
                if (was_off) {
                    stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, (cx+x)>>1, (cy+y)>>1, cx, cy);
                }
                cx = x; cy = y; was_off = 1;
            } else {
                if (was_off) stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, x, y, cx, cy);
                else stbtt_setvertex(&vertices[num_vertices++], STBTT_vline, x, y, 0, 0);
                was_off = 0;
            }
        }
    }
    *pvertices = vertices;
    return num_vertices;
}

/* ---------------------------------------------------------------------------
 * Curve flattening
 * --------------------------------------------------------------------------- */
typedef struct { float x, y; } stbtt__point;

static void stbtt__add_point(stbtt__point *pts, int n, float x, float y) {
    if (!pts) return;
    pts[n].x = x; pts[n].y = y;
}

static int stbtt__tesselate_curve(stbtt__point *pts, int *num_points, float x0, float y0, float x1, float y1, float x2, float y2, float objspace_flatness_squared, int n) {
    float mx = (x0 + 2*x1 + x2)/4, my = (y0 + 2*y1 + y2)/4;
    float dx = (x0+x2)/2 - mx, dy = (y0+y2)/2 - my;
    if (n > 16) return 1;
    if (dx*dx+dy*dy > objspace_flatness_squared) {
        stbtt__tesselate_curve(pts, num_points, x0, y0, (x0+x1)/2.0f, (y0+y1)/2.0f, mx, my, objspace_flatness_squared, n+1);
        stbtt__tesselate_curve(pts, num_points, mx, my, (x1+x2)/2.0f, (y1+y2)/2.0f, x2, y2, objspace_flatness_squared, n+1);
    } else {
        stbtt__add_point(pts, *num_points, x2, y2);
        *num_points = *num_points + 1;
    }
    return 1;
}

static stbtt__point *stbtt_FlattenCurves(stbtt_vertex *vertices, int num_verts, float objspace_flatness, int **contour_lengths, int *num_contours, void *userdata) {
    int n = 0;
    for (int i = 0; i < num_verts; i++)
        if (vertices[i].type == STBTT_vmove) n++;
    *num_contours = n;
    if (n == 0) { *contour_lengths = NULL; return NULL; }
    *contour_lengths = (int *)STBTT_malloc(sizeof(int) * n, userdata);
    float objspace_flatness_squared = objspace_flatness * objspace_flatness;
    int num_points = 0;
    /* first pass: count */
    for (int pass = 0; pass < 2; pass++) {
        float x = 0, y = 0;
        stbtt__point *points = NULL;
        if (pass == 1) points = (stbtt__point *)STBTT_malloc(num_points * sizeof(stbtt__point), userdata);
        num_points = 0; n = -1;
        int start = 0;
        for (int i = 0; i < num_verts; i++) {
            switch (vertices[i].type) {
                case STBTT_vmove:
                    if (n >= 0) (*contour_lengths)[n] = num_points - start;
                    n++; start = num_points;
                    x = vertices[i].x; y = vertices[i].y;
                    stbtt__add_point(points, num_points++, x, y);
                    break;
                case STBTT_vline:
                    x = vertices[i].x; y = vertices[i].y;
                    stbtt__add_point(points, num_points++, x, y);
                    break;
                case STBTT_vcurve:
                    stbtt__tesselate_curve(points, &num_points, x, y, vertices[i].cx, vertices[i].cy, vertices[i].x, vertices[i].y, objspace_flatness_squared, 0);
                    x = vertices[i].x; y = vertices[i].y;
                    break;
            }
        }
        if (n >= 0) (*contour_lengths)[n] = num_points - start;
    }
    return (stbtt__point *)STBTT_malloc(num_points * sizeof(stbtt__point), userdata);
}

/* ---------------------------------------------------------------------------
 * Rasterizer (version 2)
 * --------------------------------------------------------------------------- */
typedef struct {
    int w, h, stride;
    unsigned char *pixels;
} stbtt__bitmap;

typedef struct stbtt__hheap_chunk { struct stbtt__hheap_chunk *next; } stbtt__hheap_chunk;
typedef struct { stbtt__hheap_chunk *head; void *first_free; int num_remaining_in_head_chunk; } stbtt__hheap;

static void *stbtt__hheap_alloc(stbtt__hheap *hh, size_t size, void *userdata) {
    if (hh->first_free) { void *p = hh->first_free; hh->first_free = *(void **)p; return p; }
    if (hh->num_remaining_in_head_chunk == 0) {
        int count = (size < 32 ? 2000 : size < 128 ? 800 : 100);
        stbtt__hheap_chunk *c = (stbtt__hheap_chunk *)STBTT_malloc(sizeof(*c) + size*count, userdata);
        if (!c) return NULL;
        c->next = hh->head; hh->head = c;
        hh->num_remaining_in_head_chunk = count;
    }
    hh->num_remaining_in_head_chunk--;
    return (char *)(hh->head) + sizeof(stbtt__hheap_chunk) + size * hh->num_remaining_in_head_chunk;
}

static void stbtt__hheap_free(stbtt__hheap *hh, void *p) { *(void **)p = hh->first_free; hh->first_free = p; }

static void stbtt__hheap_cleanup(stbtt__hheap *hh, void *userdata) {
    stbtt__hheap_chunk *c = hh->head;
    while (c) { stbtt__hheap_chunk *n = c->next; STBTT_free(c, userdata); c = n; }
}

typedef struct { float x0, y0, x1, y1; int invert; } stbtt__edge;
typedef struct stbtt__active_edge {
    struct stbtt__active_edge *next;
    float fx, fdx, fdy, direction, sy, ey;
} stbtt__active_edge;

static stbtt__active_edge *stbtt__new_active(stbtt__hheap *hh, stbtt__edge *e, int off_x, float start_point, void *userdata) {
    stbtt__active_edge *z = (stbtt__active_edge *)stbtt__hheap_alloc(hh, sizeof(*z), userdata);
    if (!z) return NULL;
    float dxdy = (e->x1 - e->x0) / (e->y1 - e->y0);
    z->fdx = dxdy;
    z->fdy = dxdy != 0.0f ? (1.0f/dxdy) : 0.0f;
    z->fx = e->x0 + dxdy * (start_point - e->y0) - off_x;
    z->direction = e->invert ? 1.0f : -1.0f;
    z->sy = e->y0; z->ey = e->y1;
    return z;
}

static void stbtt__handle_clipped_edge(float *scanline, int x, stbtt__active_edge *e, float x0, float y0, float x1, float y1) {
    if (y0 == y1) return;
    if (y0 > e->ey || y1 < e->sy) return;
    if (y0 < e->sy) { x0 += (x1-x0)*(e->sy-y0)/(y1-y0); y0 = e->sy; }
    if (y1 > e->ey) { x1 += (x1-x0)*(e->ey-y1)/(y1-y0); y1 = e->ey; }
    if (x0 <= x && x1 <= x) scanline[x] += e->direction * (y1-y0);
    else if (x0 >= x+1 && x1 >= x+1) return;
    else scanline[x] += e->direction * (y1-y0) * (1-((x0-x)+(x1-x))/2);
}

static void stbtt__fill_active_edges_new(float *scanline, float *scanline_fill, int len, stbtt__active_edge *e, float y_top) {
    float y_bottom = y_top + 1;
    while (e) {
        if (e->fdx == 0) {
            float x0 = e->fx;
            if (x0 < len) {
                if (x0 >= 0) stbtt__handle_clipped_edge(scanline, (int)x0, e, x0, y_top, x0, y_bottom);
                else stbtt__handle_clipped_edge(scanline_fill-1, 0, e, x0, y_top, x0, y_bottom);
            }
        } else {
            float dx = e->fdx, xb = e->fx + dx;
            float x_top = (e->sy > y_top) ? e->fx + dx*(e->sy - y_top) : e->fx;
            float sy0 = (e->sy > y_top) ? e->sy : y_top;
            float x_bottom = (e->ey < y_bottom) ? e->fx + dx*(e->ey - y_top) : xb;
            float sy1 = (e->ey < y_bottom) ? e->ey : y_bottom;
            if (x_top >= 0 && x_bottom >= 0 && x_top < len && x_bottom < len) {
                int x1 = (int)x_top, x2 = (int)x_bottom;
                if (x1 == x2) {
                    float height = (sy1-sy0)*e->direction;
                    scanline[x1] += (x_top + x_bottom)/2 * height;
                } else {
                    float y_crossing = y_top + dx*(x1+1 - e->fx);
                    if (y_crossing > y_bottom) y_crossing = y_bottom;
                    float area = e->direction * (y_crossing - sy0);
                    scanline[x1] += area * (x1+1 - x_top)/2;
                    float step = e->direction * (y_bottom - y_crossing)/(x2 - x1);
                    for (int x = x1+1; x < x2; x++) { scanline[x] += area + step/2; area += step; }
                    scanline[x2] += area + e->direction*(sy1 - y_crossing)*(x_bottom - x2 + 1)/2;
                }
            } else {
                for (int x = 0; x < len; x++) {
                    float y1 = (x - e->fx)/dx + y_top, y2 = (x+1 - e->fx)/dx + y_top;
                    if (e->fx < x+0.5f && xb > x+0.5f) {
                        float yy0 = (y1 + y2)/2;
                        scanline[x] += e->direction * (yy0 > sy0 && yy0 < sy1 ? (y1-y2) : 0);
                    }
                }
            }
        }
        e = e->next;
    }
}

static void stbtt__rasterize_sorted_edges(stbtt__bitmap *result, stbtt__edge *e, int n, int off_x, int off_y, void *userdata) {
    stbtt__hheap hh = {0}; stbtt__active_edge *active = NULL;
    int y = off_y;
    e[n].y0 = (float)(off_y + result->h) + 1;
    float *scanline = (float *)STBTT_malloc((result->w*2+1)*sizeof(float), userdata);
    float *scanline2 = scanline + result->w;

    for (int j = 0; j < result->h; j++) {
        STBTT_memset(scanline, 0, result->w*sizeof(float));
        STBTT_memset(scanline2, 0, (result->w+1)*sizeof(float));
        float scan_y_top = y + 0.0f, scan_y_bottom = y + 1.0f;
        stbtt__active_edge **step = &active;
        while (*step) {
            stbtt__active_edge *z = *step;
            if (z->ey <= scan_y_top) { *step = z->next; stbtt__hheap_free(&hh, z); }
            else step = &z->next;
        }
        while (e->y0 <= scan_y_bottom) {
            if (e->y0 != e->y1) {
                stbtt__active_edge *z = stbtt__new_active(&hh, e, off_x, scan_y_top, userdata);
                if (z) { z->next = active; active = z; }
            }
            e++;
        }
        if (active) stbtt__fill_active_edges_new(scanline, scanline2+1, result->w, active, scan_y_top);
        float sum = 0;
        for (int i = 0; i < result->w; i++) {
            sum += scanline2[i];
            float k = STBTT_fabs(scanline[i] + sum)*255 + 0.5f;
            int m = (int)k; if (m > 255) m = 255;
            result->pixels[j*result->stride + i] = (unsigned char)m;
        }
        for (step = &active; *step; step = &(*step)->next) (*step)->fx += (*step)->fdx;
        y++;
    }
    stbtt__hheap_cleanup(&hh, userdata);
    STBTT_free(scanline, userdata);
}

#define STBTT__COMPARE(a,b) ((a)->y0 < (b)->y0)

static void stbtt__sort_edges(stbtt__edge *p, int n) {
    /* insertion sort — n is always small for single glyphs */
    for (int i = 1; i < n; i++) {
        stbtt__edge t = p[i];
        int j = i;
        while (j > 0 && p[j-1].y0 > t.y0) { p[j] = p[j-1]; j--; }
        p[j] = t;
    }
}

static void stbtt__rasterize(stbtt__bitmap *result, stbtt__point *pts, int *wcount, int windings, float scale_x, float scale_y, float shift_x, float shift_y, int off_x, int off_y, int invert, void *userdata) {
    float y_scale_inv = invert ? -scale_y : scale_y;
    int n = 0;
    for (int i = 0; i < windings; i++) n += wcount[i];
    stbtt__edge *e = (stbtt__edge *)STBTT_malloc(sizeof(*e)*(n+1), userdata);
    if (!e) return;
    n = 0;
    int m = 0;
    for (int i = 0; i < windings; i++) {
        stbtt__point *p = pts + m; m += wcount[i];
        int j = wcount[i]-1;
        for (int k = 0; k < wcount[i]; j = k++) {
            if (p[j].y == p[k].y) continue;
            int a = k, b = j;
            e[n].invert = 0;
            if (invert ? p[j].y > p[k].y : p[j].y < p[k].y) { e[n].invert = 1; a = j; b = k; }
            e[n].x0 = p[a].x*scale_x + shift_x; e[n].y0 = p[a].y*y_scale_inv + shift_y;
            e[n].x1 = p[b].x*scale_x + shift_x; e[n].y1 = p[b].y*y_scale_inv + shift_y;
            n++;
        }
    }
    stbtt__sort_edges(e, n);
    stbtt__rasterize_sorted_edges(result, e, n, off_x, off_y, userdata);
    STBTT_free(e, userdata);
}

static void stbtt_Rasterize(stbtt__bitmap *result, float flatness_in_pixels, stbtt_vertex *vertices, int num_verts, float scale_x, float scale_y, float shift_x, float shift_y, int x_off, int y_off, int invert, void *userdata) {
    float scale = scale_x > scale_y ? scale_y : scale_x;
    int winding_count, *winding_lengths;
    stbtt__point *windings = stbtt_FlattenCurves(vertices, num_verts, flatness_in_pixels/scale, &winding_lengths, &winding_count, userdata);
    if (windings) {
        stbtt__rasterize(result, windings, winding_lengths, winding_count, scale_x, scale_y, shift_x, shift_y, x_off, y_off, invert, userdata);
        STBTT_free(winding_lengths, userdata);
        STBTT_free(windings, userdata);
    }
}

/* ---------------------------------------------------------------------------
 * Public bitmap API
 * --------------------------------------------------------------------------- */
void stbtt_FreeBitmap(unsigned char *bitmap, void *userdata) { STBTT_free(bitmap, userdata); }

unsigned char *stbtt_GetCodepointBitmap(const stbtt_fontinfo *info, float scale_x, float scale_y, int codepoint, int *width, int *height, int *xoff, int *yoff) {
    stbtt_vertex *vertices;
    int num_verts = stbtt_GetGlyphShape(info, stbtt_FindGlyphIndex(info, codepoint), &vertices);
    if (!num_verts) { *width = *height = 0; *xoff = *yoff = 0; return NULL; }

    int x0, y0, x1, y1;
    if (!stbtt_GetGlyphBox(info, stbtt_FindGlyphIndex(info, codepoint), &x0, &y0, &x1, &y1)) {
        STBTT_free(vertices, info->userdata);
        *width = *height = 0; return NULL;
    }

    int ix0 = STBTT_ifloor(x0*scale_x), iy0 = STBTT_ifloor(-y1*scale_y);
    int ix1 = STBTT_iceil(x1*scale_x),  iy1 = STBTT_iceil(-y0*scale_y);

    int w = ix1 - ix0, h = iy1 - iy0;
    *width = w; *height = h; *xoff = ix0; *yoff = iy0;

    if (w <= 0 || h <= 0) { STBTT_free(vertices, info->userdata); return NULL; }

    stbtt__bitmap gbm;
    gbm.w = w; gbm.h = h; gbm.stride = w;
    gbm.pixels = (unsigned char *)STBTT_malloc(w*h, info->userdata);
    if (gbm.pixels) {
        stbtt_Rasterize(&gbm, 0.35f, vertices, num_verts, scale_x, scale_y, 0, 0, ix0, iy0, 1, info->userdata);
    }
    STBTT_free(vertices, info->userdata);
    return gbm.pixels;
}

#endif /* STB_TRUETYPE_IMPLEMENTATION */