#include "animation.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static AnimationDef **registry = NULL;
static int registry_count = 0;
extern uint32_t parse_color(const char *s);

float ease_linear(float t) { return t; }
float ease_in_quad(float t) { return t * t; }
float ease_out_quad(float t) { return t * (2.0f - t); }
float ease_in_out_quad(float t) { return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t; }
float ease_in_cubic(float t) { return t * t * t; }
float ease_out_cubic(float t) { t -= 1.0f; return t * t * t + 1.0f; }
float ease_in_out_cubic(float t) { return t < 0.5f ? 4.0f * t * t * t : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f; }

float ease_bounce(float t) {
    if (t < 1.0f / 2.75f) return 7.5625f * t * t;
    if (t < 2.0f / 2.75f) { t -= 1.5f / 2.75f; return 7.5625f * t * t + 0.75f; }
    if (t < 2.5f / 2.75f) { t -= 2.25f / 2.75f; return 7.5625f * t * t + 0.9375f; }
    t -= 2.625f / 2.75f;
    return 7.5625f * t * t + 0.984375f;
}

float ease_elastic(float t) {
    if (t == 0.0f || t == 1.0f) return t;
    return powf(2.0f, -10.0f * t) * sinf((t - 0.075f) * (2.0f * M_PI) / 0.3f) + 1.0f;
}

float easing_apply(EasingFunction fn, float t) {
    switch (fn) {
        case EASE_LINEAR:        return ease_linear(t);
        case EASE_IN:            return ease_in_quad(t);
        case EASE_OUT:           return ease_out_quad(t);
        case EASE_IN_OUT:        return ease_in_out_quad(t);
        case EASE_IN_CUBIC:      return ease_in_cubic(t);
        case EASE_OUT_CUBIC:     return ease_out_cubic(t);
        case EASE_IN_OUT_CUBIC:  return ease_in_out_cubic(t);
        case EASE_BOUNCE:        return ease_bounce(t);
        case EASE_ELASTIC:       return ease_elastic(t);
        default:                 return t;
    }
}

uint32_t anim_lerp_color(uint32_t a, uint32_t b, float t) {
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF, aa = (a >> 24) & 0xFF;
    int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF, ba = (b >> 24) & 0xFF;
    int r = ar + (int)((br - ar) * t);
    int g = ag + (int)((bg - ag) * t);
    int bl = ab + (int)((bb - ab) * t);
    int al = aa + (int)((ba - aa) * t);
    return (al << 24) | (r << 16) | (g << 8) | bl;
}

int anim_lerp_int(int a, int b, float t) {
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    return a + (int)((b - a) * t);
}

float anim_lerp_float(float a, float b, float t) {
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    return a + (b - a) * t;
}

Rect anim_lerp_rect(Rect *from, Rect *to, float t) {
    Rect r;
    r.x = anim_lerp_int(from->x, to->x, t);
    r.y = anim_lerp_int(from->y, to->y, t);
    r.w = anim_lerp_int(from->w, to->w, t);
    r.h = anim_lerp_int(from->h, to->h, t);
    return r;
}

WidgetStyle anim_lerp_style(WidgetStyle *from, WidgetStyle *to, float t) {
    WidgetStyle result = *from;
    if (t <= 0.0f) return *from;
    if (t >= 1.0f) return *to;

    result.fg_color = anim_lerp_color(from->fg_color, to->fg_color, t);
    result.bg_color = anim_lerp_color(from->bg_color, to->bg_color, t);
    result.border_color = anim_lerp_color(from->border_color, to->border_color, t);
    result.accent_color = anim_lerp_color(from->accent_color, to->accent_color, t);
    result.border_width = anim_lerp_int(from->border_width, to->border_width, t);
    result.border_radius = anim_lerp_int(from->border_radius, to->border_radius, t);
    result.font_size = anim_lerp_int(from->font_size, to->font_size, t);
    result.font_weight = anim_lerp_int(from->font_weight, to->font_weight, t);
    result.opacity = anim_lerp_float(from->opacity, to->opacity, t);
    result.scale_x = anim_lerp_float(from->scale_x, to->scale_x, t);
    result.scale_y = anim_lerp_float(from->scale_y, to->scale_y, t);
    result.rotation = anim_lerp_float(from->rotation, to->rotation, t);
    result.translate_x = anim_lerp_float(from->translate_x, to->translate_x, t);
    result.translate_y = anim_lerp_float(from->translate_y, to->translate_y, t);
    result.backdrop_blur = anim_lerp_int(from->backdrop_blur, to->backdrop_blur, t);

    int max_shadows = from->shadow_count > to->shadow_count ? from->shadow_count : to->shadow_count;
    result.shadow_count = max_shadows;
    for (int i = 0; i < max_shadows; i++) {
        ShadowLayer *sf = (i < from->shadow_count) ? &from->shadows[i] : &from->shadows[from->shadow_count - 1];
        ShadowLayer *st = (i < to->shadow_count) ? &to->shadows[i] : &to->shadows[to->shadow_count - 1];
        result.shadows[i].offset_x = anim_lerp_int(sf->offset_x, st->offset_x, t);
        result.shadows[i].offset_y = anim_lerp_int(sf->offset_y, st->offset_y, t);
        result.shadows[i].blur = anim_lerp_int(sf->blur, st->blur, t);
        result.shadows[i].spread = anim_lerp_int(sf->spread, st->spread, t);
        result.shadows[i].color = anim_lerp_color(sf->color, st->color, t);
    }

    if (from->gradient.type != GRADIENT_NONE && to->gradient.type != GRADIENT_NONE) {
        result.gradient.type = to->gradient.type;
        result.gradient.angle = anim_lerp_float(from->gradient.angle, to->gradient.angle, t);
        result.gradient.center_x = anim_lerp_float(from->gradient.center_x, to->gradient.center_x, t);
        result.gradient.center_y = anim_lerp_float(from->gradient.center_y, to->gradient.center_y, t);
        int stops = from->gradient.stop_count > to->gradient.stop_count ? from->gradient.stop_count : to->gradient.stop_count;
        result.gradient.stop_count = stops;
        for (int i = 0; i < stops && i < MAX_GRADIENT_STOPS; i++) {
            GradientStop *gf = (i < from->gradient.stop_count) ? &from->gradient.stops[i] : &from->gradient.stops[from->gradient.stop_count - 1];
            GradientStop *gt = (i < to->gradient.stop_count) ? &to->gradient.stops[i] : &to->gradient.stops[to->gradient.stop_count - 1];
            result.gradient.stops[i].offset = anim_lerp_float(gf->offset, gt->offset, t);
            result.gradient.stops[i].color = anim_lerp_color(gf->color, gt->color, t);
        }
    }

    for (int i = 0; i < 4; i++) {
        result.padding[i] = anim_lerp_int(from->padding[i], to->padding[i], t);
        result.margin[i] = anim_lerp_int(from->margin[i], to->margin[i], t);
    }

    return result;
}

void animation_registry_register(const char *name, AnimationDef *def) {
    (void)name;
    registry_count++;
    registry = realloc(registry, registry_count * sizeof(AnimationDef *));
    registry[registry_count - 1] = def;
}

AnimationDef *animation_registry_lookup(const char *name) {
    for (int i = 0; i < registry_count; i++)
        if (strcmp(registry[i]->name, name) == 0) return registry[i];
    return NULL;
}

void animation_registry_clear(void) {
    for (int i = 0; i < registry_count; i++) {
        AnimationDef *def = registry[i];
        free(def->name);
        free(def->trigger_on_complete);
        free(def->fil_on_complete);
        free(def->keyframes);
        free(def);
    }
    free(registry);
    registry = NULL;
    registry_count = 0;
}

static EasingFunction parse_easing(const char *s) {
    if (!s) return EASE_LINEAR;
    if (strcmp(s, "ease-in") == 0) return EASE_IN;
    if (strcmp(s, "ease-out") == 0) return EASE_OUT;
    if (strcmp(s, "ease-in-out") == 0) return EASE_IN_OUT;
    if (strcmp(s, "ease-in-cubic") == 0) return EASE_IN_CUBIC;
    if (strcmp(s, "ease-out-cubic") == 0) return EASE_OUT_CUBIC;
    if (strcmp(s, "ease-in-out-cubic") == 0) return EASE_IN_OUT_CUBIC;
    if (strcmp(s, "bounce") == 0) return EASE_BOUNCE;
    if (strcmp(s, "elastic") == 0) return EASE_ELASTIC;
    return EASE_LINEAR;
}

static TuiAnimType parse_tui_type(const char *s) {
    if (!s) return TUI_ANIM_NONE;
    if (strcmp(s, "slide-in") == 0) return TUI_ANIM_SLIDE_IN;
    if (strcmp(s, "fade") == 0) return TUI_ANIM_FADE;
    if (strcmp(s, "typewriter") == 0) return TUI_ANIM_TYPEWRITER;
    return TUI_ANIM_NONE;
}

void animation_registry_load_from_theme(cJSON *animations_json) {
    if (!animations_json || !cJSON_IsObject(animations_json)) return;
    cJSON *child = animations_json->child;
    while (child) {
        const char *name = child->string;
        cJSON *dur = cJSON_GetObjectItem(child, "duration");
        cJSON *loop = cJSON_GetObjectItem(child, "loop");
        cJSON *repeat = cJSON_GetObjectItem(child, "repeat");
        cJSON *auto_rev = cJSON_GetObjectItem(child, "auto_reverse");
        cJSON *trigger = cJSON_GetObjectItem(child, "on_complete");
        cJSON *fil_comp = cJSON_GetObjectItem(child, "fil_on_complete");
        cJSON *tui_type = cJSON_GetObjectItem(child, "tui");
        cJSON *keyframes_arr = cJSON_GetObjectItem(child, "keyframes");

        if (!keyframes_arr || !cJSON_IsArray(keyframes_arr)) { child = child->next; continue; }

        int kf_count = cJSON_GetArraySize(keyframes_arr);
        AnimationDef *def = calloc(1, sizeof(AnimationDef));
        def->name = strdup(name);
        def->duration_ms = dur ? dur->valueint : 300;
        def->loop = loop ? (loop->type == cJSON_True) : false;
        def->repeat_count = repeat ? repeat->valueint : 1;
        def->auto_reverse = auto_rev ? (auto_rev->type == cJSON_True) : false;
        if (trigger && trigger->valuestring) def->trigger_on_complete = strdup(trigger->valuestring);
        if (fil_comp && fil_comp->valuestring) def->fil_on_complete = strdup(fil_comp->valuestring);
        if (tui_type && tui_type->valuestring) def->tui_type = parse_tui_type(tui_type->valuestring);
        def->keyframe_count = kf_count;
        def->keyframes = calloc(kf_count, sizeof(AnimKeyframe));

        for (int i = 0; i < kf_count; i++) {
            cJSON *kf = cJSON_GetArrayItem(keyframes_arr, i);
            AnimKeyframe *k = &def->keyframes[i];
            cJSON *time = cJSON_GetObjectItem(kf, "time");
            k->time_offset = time ? (float)time->valuedouble : 0.0f;
            cJSON *easing = cJSON_GetObjectItem(kf, "easing");
            k->easing = easing && easing->valuestring ? parse_easing(easing->valuestring) : EASE_LINEAR;

            cJSON *fg = cJSON_GetObjectItem(kf, "fg_color");
            cJSON *bg = cJSON_GetObjectItem(kf, "bg_color");
            cJSON *border = cJSON_GetObjectItem(kf, "border_color");
            cJSON *accent = cJSON_GetObjectItem(kf, "accent_color");
            cJSON *bw = cJSON_GetObjectItem(kf, "border_width");
            cJSON *br = cJSON_GetObjectItem(kf, "border_radius");
            cJSON *fs = cJSON_GetObjectItem(kf, "font_size");
            cJSON *fw = cJSON_GetObjectItem(kf, "font_weight");
            cJSON *op = cJSON_GetObjectItem(kf, "opacity");
            cJSON *sx = cJSON_GetObjectItem(kf, "scale_x");
            cJSON *sy = cJSON_GetObjectItem(kf, "scale_y");
            cJSON *rot = cJSON_GetObjectItem(kf, "rotation");
            cJSON *tx = cJSON_GetObjectItem(kf, "translate_x");
            cJSON *ty = cJSON_GetObjectItem(kf, "translate_y");
            cJSON *rx = cJSON_GetObjectItem(kf, "x");
            cJSON *ry = cJSON_GetObjectItem(kf, "y");
            cJSON *rw = cJSON_GetObjectItem(kf, "w");
            cJSON *rh = cJSON_GetObjectItem(kf, "h");

            if (fg && fg->valuestring) { k->fg_color = parse_color(fg->valuestring); k->has_fg = true; }
            if (bg && bg->valuestring) { k->bg_color = parse_color(bg->valuestring); k->has_bg = true; }
            if (border && border->valuestring) { k->border_color = parse_color(border->valuestring); k->has_border = true; }
            if (accent && accent->valuestring) { k->accent_color = parse_color(accent->valuestring); k->has_accent = true; }
            if (bw) { k->border_width = bw->valueint; k->has_border_width = true; }
            if (br) { k->border_radius = br->valueint; k->has_border_radius = true; }
            if (fs) { k->font_size = fs->valueint; k->has_font_size = true; }
            if (fw) { k->font_weight = fw->valueint; k->has_font_weight = true; }
            if (op) { k->opacity = (float)op->valuedouble; k->has_opacity = true; }
            if (sx) { k->scale_x = (float)sx->valuedouble; k->has_scale = true; }
            if (sy) { k->scale_y = (float)sy->valuedouble; k->has_scale = true; }
            if (rot) { k->rotation = (float)rot->valuedouble; k->has_rotation = true; }
            if (tx) { k->translate_x = (float)tx->valuedouble; k->has_translate = true; }
            if (ty) { k->translate_y = (float)ty->valuedouble; k->has_translate = true; }
            if (rx) { k->rect_x = rx->valueint; k->has_rect = true; }
            if (ry) { k->rect_y = ry->valueint; k->has_rect = true; }
            if (rw) { k->rect_w = rw->valueint; k->has_rect = true; }
            if (rh) { k->rect_h = rh->valueint; k->has_rect = true; }
        }

        animation_registry_register(name, def);
        child = child->next;
    }
}

void animation_free_instances(RenderTree *tree) {
    if (!tree) return;
    for (int i = 0; i < tree->animation_count; i++) {
        free(tree->active_animations[i].animation_name);
    }
    free(tree->active_animations);
    tree->active_animations = NULL;
    tree->animation_count = 0;
}

void animation_play(RenderTree *tree, const char *name, long long now_ms) {
    AnimationDef *def = animation_registry_lookup(name);
    if (!def) return;
    animation_play_custom(tree, def, now_ms);
}

void animation_play_custom(RenderTree *tree, AnimationDef *def, long long now_ms) {
    if (!tree || !def) return;
    for (int i = 0; i < tree->animation_count; i++) {
        if (strcmp(tree->active_animations[i].animation_name, def->name) == 0) {
            tree->active_animations[i].start_time = now_ms;
            tree->active_animations[i].playing = true;
            tree->active_animations[i].paused = false;
            tree->active_animations[i].segment_progress = 0.0f;
            tree->active_animations[i].current_keyframe = 0;
            tree->active_animations[i].next_keyframe = def->keyframe_count > 1 ? 1 : 0;
            tree->active_animations[i].loop_iteration = 0;
            tree->active_animations[i].reversed = false;
            tree->active_animations[i].typewriter_pos = 0;
            tree->active_animations[i].typewriter_last_ms = now_ms;
            return;
        }
    }

    tree->animation_count++;
    tree->active_animations = realloc(tree->active_animations, tree->animation_count * sizeof(AnimInstance));
    AnimInstance *inst = &tree->active_animations[tree->animation_count - 1];
    memset(inst, 0, sizeof(*inst));
    inst->animation_name = strdup(def->name);
    inst->start_time = now_ms;
    inst->playing = true;
    inst->speed = 1.0f;
    inst->def = def;
    inst->next_keyframe = def->keyframe_count > 1 ? 1 : 0;
    inst->typewriter_pos = 0;
    inst->typewriter_last_ms = now_ms;
}

void animation_stop(RenderTree *tree, const char *name) {
    if (!tree || !name) return;
    for (int i = 0; i < tree->animation_count; i++) {
        if (strcmp(tree->active_animations[i].animation_name, name) == 0) {
            tree->active_animations[i].playing = false;
            return;
        }
    }
}

void animation_pause(RenderTree *tree, const char *name) {
    if (!tree || !name) return;
    for (int i = 0; i < tree->animation_count; i++) {
        if (strcmp(tree->active_animations[i].animation_name, name) == 0) {
            tree->active_animations[i].paused = true;
            return;
        }
    }
}

void animation_resume(RenderTree *tree, const char *name, long long now_ms) {
    if (!tree || !name) return;
    for (int i = 0; i < tree->animation_count; i++) {
        if (strcmp(tree->active_animations[i].animation_name, name) == 0) {
            tree->active_animations[i].paused = false;
            tree->active_animations[i].start_time = now_ms -
                (long long)(tree->active_animations[i].segment_progress * tree->active_animations[i].def->duration_ms);
            return;
        }
    }
}

void animation_slide_init(RenderTree *tree, const char *name, int from_x, int from_y) {
    if (!tree || !name) return;
    for (int i = 0; i < tree->animation_count; i++) {
        if (strcmp(tree->active_animations[i].animation_name, name) == 0) {
            tree->active_animations[i].slide_origin_x = from_x;
            tree->active_animations[i].slide_origin_y = from_y;
            return;
        }
    }
}

void animation_update(RenderTree *tree, long long now_ms, bool prefers_reduced_motion) {
    if (!tree || tree->animation_count == 0) return;
    if (prefers_reduced_motion) {
        for (int i = 0; i < tree->animation_count; i++) {
            AnimInstance *inst = &tree->active_animations[i];
            if (inst->playing && !inst->paused && inst->def && inst->def->keyframe_count > 0) {
                AnimKeyframe *last = &inst->def->keyframes[inst->def->keyframe_count - 1];
                WidgetStyle to_style = tree->resolved_style;
                if (last->has_fg) to_style.fg_color = last->fg_color;
                if (last->has_bg) to_style.bg_color = last->bg_color;
                if (last->has_border) to_style.border_color = last->border_color;
                if (last->has_accent) to_style.accent_color = last->accent_color;
                if (last->has_border_width) to_style.border_width = last->border_width;
                if (last->has_border_radius) to_style.border_radius = last->border_radius;
                if (last->has_font_size) to_style.font_size = last->font_size;
                if (last->has_font_weight) to_style.font_weight = last->font_weight;
                if (last->has_opacity) to_style.opacity = last->opacity;
                if (last->has_scale) { to_style.scale_x = last->scale_x; to_style.scale_y = last->scale_y; }
                if (last->has_rotation) to_style.rotation = last->rotation;
                if (last->has_translate) { to_style.translate_x = last->translate_x; to_style.translate_y = last->translate_y; }
                if (last->has_rect) {
                    tree->rect.x = last->rect_x;
                    tree->rect.y = last->rect_y;
                    tree->rect.w = last->rect_w;
                    tree->rect.h = last->rect_h;
                }
                tree->resolved_style = to_style;
                inst->playing = false;
                tree->dirty = true;
            }
        }
        return;
    }

    for (int i = 0; i < tree->animation_count; i++) {
        AnimInstance *inst = &tree->active_animations[i];
        if (!inst->playing || inst->paused) continue;

        AnimationDef *def = inst->def;
        if (!def || def->keyframe_count == 0) continue;

        long long elapsed = now_ms - inst->start_time;
        float total_progress = (float)elapsed / (float)def->duration_ms * inst->speed;
        if (total_progress < 0.0f) total_progress = 0.0f;

        if (def->auto_reverse && !inst->reversed && total_progress >= 1.0f) {
            inst->reversed = true;
            inst->start_time = now_ms;
            total_progress = 0.0f;
        }

        if (def->auto_reverse && inst->reversed && total_progress >= 1.0f) {
            inst->reversed = false;
            inst->start_time = now_ms;
            total_progress = 0.0f;
            inst->loop_iteration++;
        }

        if (!def->auto_reverse && total_progress >= 1.0f) {
            if (def->loop) {
                if (def->repeat_count < 0 || inst->loop_iteration < def->repeat_count - 1) {
                    inst->start_time = now_ms;
                    total_progress = 0.0f;
                    inst->loop_iteration++;
                } else {
                    inst->playing = false;
                    tree->dirty = true;
                    continue;
                }
            } else {
                inst->playing = false;
                tree->dirty = true;
                continue;
            }
        }

        if (total_progress > 1.0f) total_progress = 1.0f;

        int kf_from = 0;
        int kf_to = def->keyframe_count - 1;
        for (int k = 0; k < def->keyframe_count - 1; k++) {
            float start = def->keyframes[k].time_offset / (float)def->duration_ms;
            float end = def->keyframes[k + 1].time_offset / (float)def->duration_ms;
            if (total_progress >= start && total_progress <= end) {
                kf_from = k;
                kf_to = k + 1;
                break;
            }
        }

        float start_time = def->keyframes[kf_from].time_offset / (float)def->duration_ms;
        float end_time = def->keyframes[kf_to].time_offset / (float)def->duration_ms;
        float segment_duration = end_time - start_time;
        float segment_t = segment_duration > 0.0f ? (total_progress - start_time) / segment_duration : 1.0f;

        EasingFunction easing = def->keyframes[kf_from].easing;
        float eased_t = easing_apply(easing, segment_t);

        WidgetStyle from_style = tree->resolved_style;
        WidgetStyle to_style = tree->resolved_style;

        AnimKeyframe *fk = &def->keyframes[kf_from];
        AnimKeyframe *tk = &def->keyframes[kf_to];

        if (fk->has_fg) from_style.fg_color = fk->fg_color;
        if (fk->has_bg) from_style.bg_color = fk->bg_color;
        if (fk->has_border) from_style.border_color = fk->border_color;
        if (fk->has_accent) from_style.accent_color = fk->accent_color;
        if (fk->has_border_width) from_style.border_width = fk->border_width;
        if (fk->has_border_radius) from_style.border_radius = fk->border_radius;
        if (fk->has_font_size) from_style.font_size = fk->font_size;
        if (fk->has_font_weight) from_style.font_weight = fk->font_weight;
        if (fk->has_opacity) from_style.opacity = fk->opacity;
        if (fk->has_scale) { from_style.scale_x = fk->scale_x; from_style.scale_y = fk->scale_y; }
        if (fk->has_rotation) from_style.rotation = fk->rotation;
        if (fk->has_translate) { from_style.translate_x = fk->translate_x; from_style.translate_y = fk->translate_y; }

        if (tk->has_fg) to_style.fg_color = tk->fg_color;
        if (tk->has_bg) to_style.bg_color = tk->bg_color;
        if (tk->has_border) to_style.border_color = tk->border_color;
        if (tk->has_accent) to_style.accent_color = tk->accent_color;
        if (tk->has_border_width) to_style.border_width = tk->border_width;
        if (tk->has_border_radius) to_style.border_radius = tk->border_radius;
        if (tk->has_font_size) to_style.font_size = tk->font_size;
        if (tk->has_font_weight) to_style.font_weight = tk->font_weight;
        if (tk->has_opacity) to_style.opacity = tk->opacity;
        if (tk->has_scale) { to_style.scale_x = tk->scale_x; to_style.scale_y = tk->scale_y; }
        if (tk->has_rotation) to_style.rotation = tk->rotation;
        if (tk->has_translate) { to_style.translate_x = tk->translate_x; to_style.translate_y = tk->translate_y; }

        if (fk->has_rect || tk->has_rect) {
            Rect from_rect = tree->rect;
            Rect to_rect = tree->rect;
            if (fk->has_rect) { from_rect.x = fk->rect_x; from_rect.y = fk->rect_y; from_rect.w = fk->rect_w; from_rect.h = fk->rect_h; }
            if (tk->has_rect) { to_rect.x = tk->rect_x; to_rect.y = tk->rect_y; to_rect.w = tk->rect_w; to_rect.h = tk->rect_h; }
            Rect r = anim_lerp_rect(&from_rect, &to_rect, eased_t);
            tree->rect.x = r.x;
            tree->rect.y = r.y;
            tree->rect.w = r.w;
            tree->rect.h = r.h;
        }

        WidgetStyle result = anim_lerp_style(&from_style, &to_style, eased_t);
        tree->resolved_style = result;

        inst->current_keyframe = kf_from;
        inst->next_keyframe = kf_to;
        inst->segment_progress = segment_t;

        if (def->tui_type == TUI_ANIM_TYPEWRITER) {
            if (now_ms - inst->typewriter_last_ms >= 30) {
                inst->typewriter_pos++;
                inst->typewriter_last_ms = now_ms;
            }
        }

        tree->dirty = true;
    }
}