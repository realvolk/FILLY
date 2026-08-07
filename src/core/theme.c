#include "theme.h"
#include "script/fil.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <math.h>
#include "core/animation.h"

uint32_t parse_color(const char *s) {
    if (!s) return 0xFF000000;
    if (s[0] == '#') s++;
    unsigned int r = 0, g = 0, b = 0, a = 255;
    int len = strlen(s);
    if (len == 6) sscanf(s, "%02x%02x%02x", &r, &g, &b);
    else if (len == 8) sscanf(s, "%02x%02x%02x%02x", &r, &g, &b, &a);
    return (a << 24) | (r << 16) | (g << 8) | b;
}

static char *color_to_hex(uint32_t c) {
    char *hex = malloc(10);
    snprintf(hex, 10, "#%02x%02x%02x%02x", (c>>24)&0xFF, (c>>16)&0xFF, (c>>8)&0xFF, c&0xFF);
    return hex;
}

static int clamp_int(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static float clamp_float(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static uint32_t color_lighten(uint32_t c, float amount) {
    int r = ((c>>16)&0xFF) + (int)(255*amount);
    int g = ((c>>8)&0xFF) + (int)(255*amount);
    int b = (c&0xFF) + (int)(255*amount);
    int a = (c>>24)&0xFF;
    r = clamp_int(r, 0, 255); g = clamp_int(g, 0, 255); b = clamp_int(b, 0, 255);
    return (a<<24)|(r<<16)|(g<<8)|b;
}

static uint32_t color_darken(uint32_t c, float amount) {
    int r = ((c>>16)&0xFF) - (int)(255*amount);
    int g = ((c>>8)&0xFF) - (int)(255*amount);
    int b = (c&0xFF) - (int)(255*amount);
    int a = (c>>24)&0xFF;
    r = clamp_int(r, 0, 255); g = clamp_int(g, 0, 255); b = clamp_int(b, 0, 255);
    return (a<<24)|(r<<16)|(g<<8)|b;
}

static uint32_t color_alpha(uint32_t c, float amount) {
    int a = clamp_int((int)(((c>>24)&0xFF) * amount), 0, 255);
    return (a<<24)|(c&0x00FFFFFF);
}

static uint32_t color_mix(uint32_t c1, uint32_t c2, float t) {
    t = clamp_float(t, 0.0f, 1.0f);
    int r = (int)(((c1>>16)&0xFF)*(1-t) + ((c2>>16)&0xFF)*t);
    int g = (int)(((c1>>8)&0xFF)*(1-t) + ((c2>>8)&0xFF)*t);
    int b = (int)((c1&0xFF)*(1-t) + (c2&0xFF)*t);
    int a = (int)(((c1>>24)&0xFF)*(1-t) + ((c2>>24)&0xFF)*t);
    return (a<<24)|(r<<16)|(g<<8)|b;
}

static char *resolve_var(Theme *t, const char *value);

static char *apply_color_function(const char *func_name, const char *arg, Theme *t) {
    char *resolved = resolve_var(t, arg);
    if (!resolved) return strdup("#00000000");
    uint32_t base = parse_color(resolved);
    free(resolved);
    if (strcmp(func_name, "lighten") == 0) return color_to_hex(color_lighten(base, 0.2f));
    if (strcmp(func_name, "darken") == 0) return color_to_hex(color_darken(base, 0.2f));
    if (strcmp(func_name, "alpha") == 0) return color_to_hex(color_alpha(base, 0.5f));
    return strdup(arg);
}

static char *resolve_var(Theme *t, const char *value) {
    if (!value) return strdup("");
    if (value[0] != '$') return strdup(value);
    const char *name = value + 1;
    if (strncmp(name, "lighten(", 8) == 0) {
        char *paren = strchr((char*)name + 8, ')');
        if (paren) { *paren = '\0'; char *r = apply_color_function("lighten", name + 8, t); *paren = ')'; return r; }
    }
    if (strncmp(name, "darken(", 7) == 0) {
        char *paren = strchr((char*)name + 7, ')');
        if (paren) { *paren = '\0'; char *r = apply_color_function("darken", name + 7, t); *paren = ')'; return r; }
    }
    if (strncmp(name, "alpha(", 6) == 0) {
        char *paren = strchr((char*)name + 6, ')');
        if (paren) { *paren = '\0'; char *r = apply_color_function("alpha", name + 6, t); *paren = ')'; return r; }
    }
    if (strncmp(name, "mix(", 4) == 0) {
        char *comma1 = strchr((char*)name + 4, ',');
        char *comma2 = comma1 ? strchr(comma1 + 1, ',') : NULL;
        char *paren = comma2 ? strchr(comma2 + 1, ')') : NULL;
        if (comma1 && comma2 && paren) {
            *comma1 = '\0'; *comma2 = '\0'; *paren = '\0';
            uint32_t c1 = parse_color(resolve_var(t, name + 4));
            uint32_t c2 = parse_color(resolve_var(t, comma1 + 1));
            float amt = atof(comma2 + 1);
            *comma1 = ','; *comma2 = ','; *paren = ')';
            return color_to_hex(color_mix(c1, c2, amt));
        }
    }
    for (int i = 0; i < t->var_count; i++)
        if (strcmp(t->vars[i].name, name) == 0) return strdup(t->vars[i].value);
    return strdup(value);
}

static int parse_int(cJSON *o, const char *key, int def) {
    cJSON *item = cJSON_GetObjectItem(o, key);
    if (item && item->type == cJSON_Number) return item->valueint;
    return def;
}

static float parse_float(cJSON *o, const char *key, float def) {
    cJSON *item = cJSON_GetObjectItem(o, key);
    if (item && item->type == cJSON_Number) return (float)item->valuedouble;
    return def;
}

static bool parse_bool(cJSON *o, const char *key, bool def) {
    cJSON *item = cJSON_GetObjectItem(o, key);
    if (item && (item->type == cJSON_True || item->type == cJSON_False))
        return item->type == cJSON_True;
    return def;
}

static Alignment parse_align(const char *s) {
    if (!s) return ALIGN_LEFT;
    if (strcmp(s, "center") == 0) return ALIGN_CENTER;
    if (strcmp(s, "right") == 0) return ALIGN_RIGHT;
    return ALIGN_LEFT;
}

static BorderStyle parse_border_style(const char *s) {
    if (!s) return BORDER_SOLID;
    if (strcmp(s, "none") == 0) return BORDER_NONE;
    if (strcmp(s, "solid") == 0) return BORDER_SOLID;
    if (strcmp(s, "dashed") == 0) return BORDER_DASHED;
    if (strcmp(s, "dotted") == 0) return BORDER_DOTTED;
    if (strcmp(s, "double") == 0) return BORDER_DOUBLE;
    if (strcmp(s, "rounded") == 0) return BORDER_ROUNDED;
    return BORDER_SOLID;
}

static CursorStyle parse_cursor_style(const char *s) {
    if (!s) return CURSOR_DEFAULT;
    if (strcmp(s, "default") == 0) return CURSOR_DEFAULT;
    if (strcmp(s, "pointer") == 0) return CURSOR_POINTER;
    if (strcmp(s, "text") == 0) return CURSOR_TEXT;
    if (strcmp(s, "move") == 0) return CURSOR_MOVE;
    if (strcmp(s, "not-allowed") == 0) return CURSOR_NOT_ALLOWED;
    return CURSOR_DEFAULT;
}

static void parse_shadow_array(cJSON *obj, WidgetStyle *s, Theme *t) {
    cJSON *shadows = cJSON_GetObjectItem(obj, "shadow");
    if (!shadows || !cJSON_IsArray(shadows)) {
        int ox = parse_int(obj, "shadow_offset_x", 0);
        int oy = parse_int(obj, "shadow_offset_y", 0);
        int blur = parse_int(obj, "shadow_blur", 0);
        int spread = parse_int(obj, "shadow_spread", 0);
        cJSON *shcol = cJSON_GetObjectItem(obj, "shadow_color");
        if (blur > 0 || (shcol && shcol->valuestring)) {
            s->shadows[0].offset_x = ox;
            s->shadows[0].offset_y = oy;
            s->shadows[0].blur = blur;
            s->shadows[0].spread = spread;
            if (shcol && shcol->valuestring) {
                char *v = resolve_var(t, shcol->valuestring);
                s->shadows[0].color = parse_color(v);
                free(v);
            }
            s->shadow_count = 1;
        }
        return;
    }
    int count = cJSON_GetArraySize(shadows);
    if (count > MAX_SHADOWS) count = MAX_SHADOWS;
    s->shadow_count = count;
    for (int i = 0; i < count; i++) {
        cJSON *sh = cJSON_GetArrayItem(shadows, i);
        if (!sh) continue;
        s->shadows[i].offset_x = parse_int(sh, "offset_x", 0);
        s->shadows[i].offset_y = parse_int(sh, "offset_y", 2);
        s->shadows[i].blur = parse_int(sh, "blur", 4);
        s->shadows[i].spread = parse_int(sh, "spread", 0);
        cJSON *col = cJSON_GetObjectItem(sh, "color");
        if (col && col->valuestring) {
            char *v = resolve_var(t, col->valuestring);
            s->shadows[i].color = parse_color(v);
            free(v);
        } else {
            s->shadows[i].color = rgba(0, 0, 0, 80);
        }
    }
}

static void parse_gradient(cJSON *obj, WidgetStyle *s, Theme *t) {
    cJSON *grad = cJSON_GetObjectItem(obj, "bg_gradient");
    if (!grad || !cJSON_IsObject(grad)) {
        bool has = parse_bool(obj, "bg_gradient", false);
        if (has) {
            s->gradient.type = GRADIENT_LINEAR;
            s->gradient.angle = 180.0f;
            s->gradient.stop_count = 2;
            s->gradient.stops[0].offset = 0.0f;
            s->gradient.stops[0].color = s->bg_color;
            s->gradient.stops[1].offset = 1.0f;
            cJSON *gradto = cJSON_GetObjectItem(obj, "bg_gradient_to");
            if (gradto && gradto->valuestring) {
                char *v = resolve_var(t, gradto->valuestring);
                s->gradient.stops[1].color = parse_color(v);
                free(v);
            }
            s->gradient.angle = parse_float(obj, "bg_gradient_direction", 180.0f);
        }
        return;
    }
    cJSON *type = cJSON_GetObjectItem(grad, "type");
    s->gradient.type = GRADIENT_LINEAR;
    if (type && type->valuestring && strcmp(type->valuestring, "radial") == 0)
        s->gradient.type = GRADIENT_RADIAL;
    s->gradient.angle = parse_float(grad, "angle", 180.0f);
    s->gradient.center_x = parse_float(grad, "center_x", 0.5f);
    s->gradient.center_y = parse_float(grad, "center_y", 0.5f);
    cJSON *stops = cJSON_GetObjectItem(grad, "stops");
    if (stops && cJSON_IsArray(stops)) {
        int count = cJSON_GetArraySize(stops);
        if (count > MAX_GRADIENT_STOPS) count = MAX_GRADIENT_STOPS;
        s->gradient.stop_count = count;
        for (int i = 0; i < count; i++) {
            cJSON *st = cJSON_GetArrayItem(stops, i);
            if (!st) continue;
            s->gradient.stops[i].offset = parse_float(st, "offset", 0.0f);
            cJSON *col = cJSON_GetObjectItem(st, "color");
            if (col && col->valuestring) {
                char *v = resolve_var(t, col->valuestring);
                s->gradient.stops[i].color = parse_color(v);
                free(v);
            }
        }
    }
}

static WidgetStyle parse_widget_style(cJSON *obj, Theme *t) {
    WidgetStyle s = widgetstyle_default();
    cJSON *fg = cJSON_GetObjectItem(obj, "fg_color");
    cJSON *bg = cJSON_GetObjectItem(obj, "bg_color");
    cJSON *border = cJSON_GetObjectItem(obj, "border_color");
    cJSON *accent = cJSON_GetObjectItem(obj, "accent_color");
    if (fg && fg->valuestring) { char *v = resolve_var(t, fg->valuestring); s.fg_color = parse_color(v); free(v); }
    if (bg && bg->valuestring) { char *v = resolve_var(t, bg->valuestring); s.bg_color = parse_color(v); free(v); }
    if (border && border->valuestring) { char *v = resolve_var(t, border->valuestring); s.border_color = parse_color(v); free(v); }
    if (accent && accent->valuestring) { char *v = resolve_var(t, accent->valuestring); s.accent_color = parse_color(v); free(v); }
    s.border_width = parse_int(obj, "border_width", s.border_width);
    s.border_radius = parse_int(obj, "border_radius", s.border_radius);

    s.border_top_width = parse_int(obj, "border_top_width", parse_int(obj, "border_width", 1));
    s.border_bottom_width = parse_int(obj, "border_bottom_width", parse_int(obj, "border_width", 1));
    s.border_left_width = parse_int(obj, "border_left_width", parse_int(obj, "border_width", 1));
    s.border_right_width = parse_int(obj, "border_right_width", parse_int(obj, "border_width", 1));

    cJSON *btc = cJSON_GetObjectItem(obj, "border_top_color");
    if (btc && btc->valuestring) { char *v = resolve_var(t, btc->valuestring); s.border_top_color = parse_color(v); free(v); }
    else s.border_top_color = s.border_color;
    cJSON *bbc = cJSON_GetObjectItem(obj, "border_bottom_color");
    if (bbc && bbc->valuestring) { char *v = resolve_var(t, bbc->valuestring); s.border_bottom_color = parse_color(v); free(v); }
    else s.border_bottom_color = s.border_color;
    cJSON *blc = cJSON_GetObjectItem(obj, "border_left_color");
    if (blc && blc->valuestring) { char *v = resolve_var(t, blc->valuestring); s.border_left_color = parse_color(v); free(v); }
    else s.border_left_color = s.border_color;
    cJSON *brc = cJSON_GetObjectItem(obj, "border_right_color");
    if (brc && brc->valuestring) { char *v = resolve_var(t, brc->valuestring); s.border_right_color = parse_color(v); free(v); }
    else s.border_right_color = s.border_color;

    cJSON *bts = cJSON_GetObjectItem(obj, "border_top_style");
    cJSON *bbs = cJSON_GetObjectItem(obj, "border_bottom_style");
    cJSON *bls = cJSON_GetObjectItem(obj, "border_left_style");
    cJSON *brs = cJSON_GetObjectItem(obj, "border_right_style");
    s.border_top_style = bts && bts->valuestring ? parse_border_style(bts->valuestring) : BORDER_SOLID;
    s.border_bottom_style = bbs && bbs->valuestring ? parse_border_style(bbs->valuestring) : BORDER_SOLID;
    s.border_left_style = bls && bls->valuestring ? parse_border_style(bls->valuestring) : BORDER_SOLID;
    s.border_right_style = brs && brs->valuestring ? parse_border_style(brs->valuestring) : BORDER_SOLID;

    parse_shadow_array(obj, &s, t);
    parse_gradient(obj, &s, t);
    s.backdrop_blur = parse_int(obj, "backdrop_blur", 0);
    s.letter_spacing = parse_float(obj, "letter_spacing", 0.0f);
    s.line_height = parse_float(obj, "line_height", 1.2f);
    s.text_transform = parse_int(obj, "text_transform", 0);

    cJSON *pad = cJSON_GetObjectItem(obj, "padding");
    if (pad && pad->type == cJSON_Array && cJSON_GetArraySize(pad) >= 4) {
        s.padding[0] = cJSON_GetArrayItem(pad, 0)->valueint;
        s.padding[1] = cJSON_GetArrayItem(pad, 1)->valueint;
        s.padding[2] = cJSON_GetArrayItem(pad, 2)->valueint;
        s.padding[3] = cJSON_GetArrayItem(pad, 3)->valueint;
    } else if (pad && pad->type == cJSON_Number) {
        int v = pad->valueint;
        s.padding[0] = v; s.padding[1] = v; s.padding[2] = v; s.padding[3] = v;
    }
    cJSON *mar = cJSON_GetObjectItem(obj, "margin");
    if (mar && mar->type == cJSON_Array && cJSON_GetArraySize(mar) >= 4) {
        s.margin[0] = cJSON_GetArrayItem(mar, 0)->valueint;
        s.margin[1] = cJSON_GetArrayItem(mar, 1)->valueint;
        s.margin[2] = cJSON_GetArrayItem(mar, 2)->valueint;
        s.margin[3] = cJSON_GetArrayItem(mar, 3)->valueint;
    } else if (mar && mar->type == cJSON_Number) {
        int v = mar->valueint;
        s.margin[0] = v; s.margin[1] = v; s.margin[2] = v; s.margin[3] = v;
    }
    s.min_width = parse_int(obj, "min_width", s.min_width);
    s.min_height = parse_int(obj, "min_height", s.min_height);
    s.max_width = parse_int(obj, "max_width", s.max_width);
    s.max_height = parse_int(obj, "max_height", s.max_height);
    free(s.font_family);
    cJSON *ff = cJSON_GetObjectItem(obj, "font_family");
    if (ff && ff->valuestring) { char *v = resolve_var(t, ff->valuestring); s.font_family = v; }
    else s.font_family = strdup("sans");
    s.font_size = parse_int(obj, "font_size", s.font_size);
    s.font_weight = parse_int(obj, "font_weight", s.font_weight);
    s.font_italic = parse_bool(obj, "font_italic", s.font_italic);
    cJSON *align = cJSON_GetObjectItem(obj, "text_align");
    if (align && align->valuestring) s.text_align = parse_align(align->valuestring);
    s.opacity = parse_float(obj, "opacity", s.opacity);
    s.transition_ms = parse_int(obj, "transition_ms", s.transition_ms);
    cJSON *cursor = cJSON_GetObjectItem(obj, "cursor");
    if (cursor && cursor->valuestring) s.cursor_style = parse_cursor_style(cursor->valuestring);
    return s;
}

static void merge_style(WidgetStyle *dst, WidgetStyle *src) {
    if (src->fg_color != 0xFF000000) dst->fg_color = src->fg_color;
    if (src->bg_color != 0) dst->bg_color = src->bg_color;
    dst->border_color = src->border_color;
    dst->accent_color = src->accent_color;
    if (src->border_width != 1) dst->border_width = src->border_width;
    if (src->border_radius != 4) dst->border_radius = src->border_radius;
    if (src->border_top_width != 1) dst->border_top_width = src->border_top_width;
    if (src->border_bottom_width != 1) dst->border_bottom_width = src->border_bottom_width;
    if (src->border_left_width != 1) dst->border_left_width = src->border_left_width;
    if (src->border_right_width != 1) dst->border_right_width = src->border_right_width;
    if (src->border_top_color != 0) dst->border_top_color = src->border_top_color;
    if (src->border_bottom_color != 0) dst->border_bottom_color = src->border_bottom_color;
    if (src->border_left_color != 0) dst->border_left_color = src->border_left_color;
    if (src->border_right_color != 0) dst->border_right_color = src->border_right_color;
    if (src->border_top_style != BORDER_SOLID) dst->border_top_style = src->border_top_style;
    if (src->border_bottom_style != BORDER_SOLID) dst->border_bottom_style = src->border_bottom_style;
    if (src->border_left_style != BORDER_SOLID) dst->border_left_style = src->border_left_style;
    if (src->border_right_style != BORDER_SOLID) dst->border_right_style = src->border_right_style;
    if (src->padding[0] != 8 || src->padding[1] != 12 || src->padding[2] != 8 || src->padding[3] != 12) {
        dst->padding[0] = src->padding[0]; dst->padding[1] = src->padding[1];
        dst->padding[2] = src->padding[2]; dst->padding[3] = src->padding[3];
    }
    if (src->margin[0] != 0 || src->margin[1] != 0 || src->margin[2] != 0 || src->margin[3] != 0) {
        dst->margin[0] = src->margin[0]; dst->margin[1] = src->margin[1];
        dst->margin[2] = src->margin[2]; dst->margin[3] = src->margin[3];
    }
    if (src->min_width != 0) dst->min_width = src->min_width;
    if (src->min_height != 0) dst->min_height = src->min_height;
    if (src->max_width != 99999) dst->max_width = src->max_width;
    if (src->max_height != 99999) dst->max_height = src->max_height;
    if (src->font_family) { free(dst->font_family); dst->font_family = strdup(src->font_family); }
    if (src->font_size != 14) dst->font_size = src->font_size;
    if (src->font_weight != 400) dst->font_weight = src->font_weight;
    if (src->font_italic) dst->font_italic = src->font_italic;
    if (src->text_align != ALIGN_LEFT) dst->text_align = src->text_align;
    if (src->opacity != 1.0f) dst->opacity = src->opacity;
    if (src->shadow_count > 0) {
        dst->shadow_count = src->shadow_count;
        for (int i = 0; i < src->shadow_count; i++)
            dst->shadows[i] = src->shadows[i];
    }
    if (src->gradient.type != GRADIENT_NONE) {
        dst->gradient = src->gradient;
    }
    if (src->backdrop_blur != 0) dst->backdrop_blur = src->backdrop_blur;
    if (src->letter_spacing != 0.0f) dst->letter_spacing = src->letter_spacing;
    if (src->line_height != 1.2f) dst->line_height = src->line_height;
    if (src->text_transform != 0) dst->text_transform = src->text_transform;
    if (src->transition_ms != 150) dst->transition_ms = src->transition_ms;
    if (src->cursor_style != CURSOR_DEFAULT) dst->cursor_style = src->cursor_style;
}

static StyleRule *parse_style_rules(cJSON *widgets_obj, Theme *t) {
    if (!widgets_obj || widgets_obj->type != cJSON_Object) return NULL;
    StyleRule *head = NULL;
    cJSON *child = widgets_obj->child;
    while (child) {
        const char *selector = child->string;
        StyleRule *rule = calloc(1, sizeof(StyleRule));
        rule->widget_class = strdup(selector);
        char *colon = strchr(rule->widget_class, ':');
        if (colon) { *colon = '\0'; rule->state = strdup(colon + 1); }
        char *space = strchr(rule->widget_class, ' ');
        if (space) { *space = '\0'; rule->child_class = strdup(space + 1); }
        rule->style = parse_widget_style(child, t);
        rule->next = head;
        head = rule;
        child = child->next;
    }
    return head;
}

Theme *theme_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *buf = malloc(sz + 1);
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return NULL;
    Theme *t = calloc(1, sizeof(Theme));
    cJSON *name = cJSON_GetObjectItem(root, "name");
    t->name = name && name->valuestring ? strdup(name->valuestring) : strdup("unnamed");
    cJSON *extends = cJSON_GetObjectItem(root, "extends");
    t->extends = NULL;

    cJSON *vars = cJSON_GetObjectItem(root, "variables");
    if (vars && cJSON_IsObject(vars)) {
        t->var_count = 0;
        cJSON *v = vars->child;
        while (v) { t->var_count++; v = v->next; }
        t->vars = calloc(t->var_count, sizeof(Variable));
        v = vars->child;
        int vi = 0;
        while (v && vi < t->var_count) {
            t->vars[vi].name = strdup(v->string);
            t->vars[vi].value = v->valuestring ? strdup(v->valuestring) : strdup("");
            vi++; v = v->next;
        }
    }

    cJSON *widgets = cJSON_GetObjectItem(root, "widgets");
    if (widgets) t->rules = parse_style_rules(widgets, t);
    cJSON *animations = cJSON_GetObjectItem(root, "animations");
    if (animations) animation_registry_load_from_theme(animations);
    cJSON_Delete(root);

    if (extends && cJSON_IsArray(extends)) {
        int count = cJSON_GetArraySize(extends);
        for (int i = 0; i < count; i++) {
            cJSON *ext = cJSON_GetArrayItem(extends, i);
            if (ext->valuestring) {
                char base_path[4096];
                const char *last_slash = strrchr(path, '/');
                if (last_slash)
                    snprintf(base_path, sizeof(base_path), "%.*s/%s.json",
                             (int)(last_slash - path), path, ext->valuestring);
                else
                    snprintf(base_path, sizeof(base_path), "%s.json", ext->valuestring);
                Theme *base = theme_load(base_path);
                if (base) {
                    if (base->rules) {
                        StyleRule *last = base->rules;
                        while (last->next) last = last->next;
                        last->next = t->rules;
                        t->rules = base->rules;
                    }
                    if (base->var_count > 0) {
                        Variable *new_vars = malloc((base->var_count + t->var_count) * sizeof(Variable));
                        int new_count = 0;
                        for (int j = 0; j < base->var_count; j++) {
                            bool found = false;
                            for (int k = 0; k < t->var_count; k++)
                                if (strcmp(base->vars[j].name, t->vars[k].name) == 0) { found = true; break; }
                            if (!found) {
                                new_vars[new_count].name = strdup(base->vars[j].name);
                                new_vars[new_count].value = strdup(base->vars[j].value);
                                new_count++;
                            }
                        }
                        for (int j = 0; j < t->var_count; j++) {
                            new_vars[new_count].name = t->vars[j].name;
                            new_vars[new_count].value = t->vars[j].value;
                            new_count++;
                        }
                        free(t->vars); t->vars = new_vars; t->var_count = new_count;
                    }
                    free(base->name); free(base->extends); free(base);
                }
            }
        }
    } else if (extends && extends->valuestring) {
        char base_path[4096];
        const char *last_slash = strrchr(path, '/');
        if (last_slash)
            snprintf(base_path, sizeof(base_path), "%.*s/%s.json",
                     (int)(last_slash - path), path, extends->valuestring);
        else
            snprintf(base_path, sizeof(base_path), "%s.json", extends->valuestring);
        Theme *base = theme_load(base_path);
        if (base) {
            if (base->rules) {
                StyleRule *last = base->rules;
                while (last->next) last = last->next;
                last->next = t->rules;
                t->rules = base->rules;
            }
            if (base->var_count > 0) {
                Variable *new_vars = malloc((base->var_count + t->var_count) * sizeof(Variable));
                int new_count = 0;
                for (int i = 0; i < base->var_count; i++) {
                    bool found = false;
                    for (int j = 0; j < t->var_count; j++)
                        if (strcmp(base->vars[i].name, t->vars[j].name) == 0) { found = true; break; }
                    if (!found) {
                        new_vars[new_count].name = strdup(base->vars[i].name);
                        new_vars[new_count].value = strdup(base->vars[i].value);
                        new_count++;
                    }
                }
                for (int j = 0; j < t->var_count; j++) {
                    new_vars[new_count].name = t->vars[j].name;
                    new_vars[new_count].value = t->vars[j].value;
                    new_count++;
                }
                free(t->vars); t->vars = new_vars; t->var_count = new_count;
            }
            free(base->name); free(base->extends); free(base);
        }
    }
    return t;
}

Theme *theme_default(void) {
    Theme *t = calloc(1, sizeof(Theme));
    t->name = strdup("default");
    return t;
}

void theme_free(Theme *t) {
    if (!t) return;
    free(t->name);
    free(t->extends);
    StyleRule *rule = t->rules;
    while (rule) {
        StyleRule *next = rule->next;
        free(rule->widget_class); free(rule->child_class); free(rule->state);
        free(rule->style.font_family);
        free(rule);
        rule = next;
    }
    for (int i = 0; i < t->var_count; i++) { free(t->vars[i].name); free(t->vars[i].value); }
    free(t->vars);
    animation_registry_clear();
    free(t);
}

WidgetStyle theme_resolve(Theme *t, const char *widget_class, const char *child_class, const char *state) {
    WidgetStyle result = widgetstyle_default();
    if (!t) return result;
    StyleRule *rule = t->rules;
    while (rule) {
        bool wm = !rule->widget_class || strcmp(rule->widget_class, widget_class) == 0;
        bool cm = !rule->child_class || (child_class && strcmp(rule->child_class, child_class) == 0);
        bool sm = !rule->state || (state && strcmp(rule->state, state) == 0);
        if (wm && cm && sm) merge_style(&result, &rule->style);
        rule = rule->next;
    }
    return result;
}

Theme *theme_load_directory(const char *dir_path) {
    DIR *d = opendir(dir_path);
    if (!d) return NULL;
    Theme *head = NULL, *tail = NULL;
    struct dirent *entry;
    while ((entry = readdir(d))) {
        int len = strlen(entry->d_name);
        if (len > 5 && strcmp(entry->d_name + len - 5, ".json") == 0) {
            char full[2048];
            snprintf(full, sizeof(full), "%s/%s", dir_path, entry->d_name);
            Theme *t = theme_load(full);
            if (t) {
                if (!head) { head = t; tail = t; }
                else { tail->next = t; tail = t; }
            }
        }
    }
    closedir(d);
    return head;
}

void theme_add_plugin_overrides(Theme *base, const char *plugin_dir) {
    Theme *plugin_themes = theme_load_directory(plugin_dir);
    if (!plugin_themes) return;
    Theme *last = base;
    while (last->next) last = last->next;
    last->next = plugin_themes;
}

void theme_merge_app_override(Theme *base, const char *path) {
    if (!path) return;
    Theme *app = theme_load(path);
    if (!app) return;
    if (app->rules) {
        StyleRule *last = base->rules;
        if (last) { while (last->next) last = last->next; last->next = app->rules; }
        else base->rules = app->rules;
        app->rules = NULL;
    }
    if (app->var_count > 0) {
        for (int i = 0; i < app->var_count; i++) {
            bool found = false;
            for (int j = 0; j < base->var_count; j++) {
                if (strcmp(base->vars[j].name, app->vars[i].name) == 0) {
                    free(base->vars[j].value);
                    base->vars[j].value = strdup(app->vars[i].value);
                    found = true; break;
                }
            }
            if (!found) {
                base->var_count++;
                base->vars = realloc(base->vars, base->var_count * sizeof(Variable));
                base->vars[base->var_count-1].name = strdup(app->vars[i].name);
                base->vars[base->var_count-1].value = strdup(app->vars[i].value);
            }
        }
    }
    theme_free(app);
}

void theme_merge_accessibility_profile(Theme *base, const char *profile_name) {
    if (!profile_name) return;
    char path[1024];
    snprintf(path, sizeof(path), "themes/%s.json", profile_name);
    Theme *a11y = theme_load(path);
    if (!a11y) return;
    if (a11y->rules) {
        StyleRule *last = base->rules;
        if (last) { while (last->next) last = last->next; last->next = a11y->rules; }
        else base->rules = a11y->rules;
        a11y->rules = NULL;
    }
    if (a11y->var_count > 0) {
        for (int i = 0; i < a11y->var_count; i++) {
            bool found = false;
            for (int j = 0; j < base->var_count; j++) {
                if (strcmp(base->vars[j].name, a11y->vars[i].name) == 0) {
                    free(base->vars[j].value);
                    base->vars[j].value = strdup(a11y->vars[i].value);
                    found = true; break;
                }
            }
            if (!found) {
                base->var_count++;
                base->vars = realloc(base->vars, base->var_count * sizeof(Variable));
                base->vars[base->var_count-1].name = strdup(a11y->vars[i].name);
                base->vars[base->var_count-1].value = strdup(a11y->vars[i].value);
            }
        }
    }
    theme_free(a11y);
}

void theme_apply_fil_styles(Theme *t, FilResult *fr) {
    if (!t || !fr) return;
    for (int i = 0; i < fr->style_count; i++) {
        char *entry = fr->style_widget[i];
        StyleRule *rule = calloc(1, sizeof(StyleRule));
        char *copy = strdup(entry);
        char *pipe1 = strchr(copy, '|');
        if (pipe1) {
            *pipe1 = '\0';
            rule->widget_class = strdup(copy);
            char *pipe2 = strchr(pipe1 + 1, '|');
            if (pipe2) {
                *pipe2 = '\0';
                if (strlen(pipe1 + 1) > 0) rule->child_class = strdup(pipe1 + 1);
                char *pipe3 = strchr(pipe2 + 1, '|');
                if (pipe3) {
                    *pipe3 = '\0';
                    char *prop_name = pipe2 + 1, *prop_val = pipe3 + 1;
                    WidgetStyle s = widgetstyle_default();
                    if (strcmp(prop_name, "bg") == 0) s.bg_color = parse_color(prop_val);
                    else if (strcmp(prop_name, "fg") == 0) s.fg_color = parse_color(prop_val);
                    else if (strcmp(prop_name, "accent") == 0) s.accent_color = parse_color(prop_val);
                    else if (strcmp(prop_name, "border-radius") == 0) s.border_radius = atoi(prop_val);
                    else if (strcmp(prop_name, "font-size") == 0) s.font_size = atoi(prop_val);
                    rule->style = s;
                }
            }
        }
        free(copy);
        rule->next = t->rules;
        t->rules = rule;
    }
}

static Theme *theme_load_user_override(void) {
    const char *home = getenv("HOME");
    if (!home) return NULL;
    char path[1024];
    snprintf(path, sizeof(path), "%s/.config/filly/theme-override.json", home);
    return theme_load(path);
}

void theme_merge_user_override(Theme *base) {
    Theme *override = theme_load_user_override();
    if (!override) return;
    if (override->rules) {
        StyleRule *last = base->rules;
        if (last) { while (last->next) last = last->next; last->next = override->rules; }
        else base->rules = override->rules;
        override->rules = NULL;
    }
    if (override->var_count > 0) {
        for (int i = 0; i < override->var_count; i++) {
            bool found = false;
            for (int j = 0; j < base->var_count; j++) {
                if (strcmp(base->vars[j].name, override->vars[i].name) == 0) {
                    free(base->vars[j].value);
                    base->vars[j].value = strdup(override->vars[i].value);
                    found = true; break;
                }
            }
            if (!found) {
                base->var_count++;
                base->vars = realloc(base->vars, base->var_count * sizeof(Variable));
                base->vars[base->var_count-1].name = strdup(override->vars[i].name);
                base->vars[base->var_count-1].value = strdup(override->vars[i].value);
            }
        }
    }
    theme_free(override);
}