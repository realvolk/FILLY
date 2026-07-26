#pragma once
#include "render.h"
#include "cJSON.h"
#include <stdbool.h>

typedef enum {
    EASE_LINEAR,
    EASE_IN,
    EASE_OUT,
    EASE_IN_OUT,
    EASE_BOUNCE,
    EASE_ELASTIC
} EasingFunction;

typedef struct AnimKeyframe {
    float time_offset;
    EasingFunction easing;
    uint32_t fg_color, bg_color, border_color, accent_color;
    int border_width, border_radius;
    int padding[4], margin[4];
    int font_size, font_weight;
    float opacity;
    int shadow_offset_x, shadow_offset_y, shadow_blur;
    uint32_t shadow_color;
    uint32_t bg_gradient_to;
    float scale_x, scale_y;
    float rotation;
    float translate_x, translate_y;
    bool has_fg, has_bg, has_border, has_accent;
    bool has_border_width, has_border_radius;
    bool has_padding, has_margin;
    bool has_font_size, has_font_weight;
    bool has_opacity;
    bool has_shadow;
    bool has_gradient;
    bool has_scale, has_rotation, has_translate;
} AnimKeyframe;

typedef struct AnimationDef {
    char *name;
    AnimKeyframe *keyframes;
    int keyframe_count;
    int duration_ms;
    bool loop;
    int repeat_count;
    bool auto_reverse;
    char *trigger_on_complete;
    char *fil_on_complete;
} AnimationDef;

typedef struct AnimInstance {
    char *animation_name;
    long long start_time;
    bool playing;
    bool paused;
    float speed;
    int current_keyframe;
    int next_keyframe;
    float segment_progress;
    int loop_iteration;
    bool reversed;
    AnimationDef *def;
} AnimInstance;

float ease_linear(float t);
float ease_in_quad(float t);
float ease_out_quad(float t);
float ease_in_out_quad(float t);
float ease_in_cubic(float t);
float ease_out_cubic(float t);
float ease_in_out_cubic(float t);
float ease_bounce(float t);
float ease_elastic(float t);
float easing_apply(EasingFunction fn, float t);

void animation_registry_register(const char *name, AnimationDef *def);
AnimationDef *animation_registry_lookup(const char *name);
void animation_registry_clear(void);
void animation_registry_load_from_theme(cJSON *animations_json);

void animation_update(RenderTree *tree, long long now_ms);
void animation_play(RenderTree *tree, const char *name, long long now_ms);
void animation_stop(RenderTree *tree, const char *name);
void animation_pause(RenderTree *tree, const char *name);
void animation_resume(RenderTree *tree, const char *name, long long now_ms);
void animation_play_custom(RenderTree *tree, AnimationDef *def, long long now_ms);
void animation_free_instances(RenderTree *tree);

uint32_t anim_lerp_color(uint32_t a, uint32_t b, float t);
int anim_lerp_int(int a, int b, float t);
float anim_lerp_float(float a, float b, float t);
WidgetStyle anim_lerp_style(WidgetStyle *from, WidgetStyle *to, float t);