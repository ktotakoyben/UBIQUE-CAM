#include "cam-morph-common.h"
#include <graphics/graphics.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-cam-morph", "ru-RU")

struct tex_slot {
    char *path;
    gs_texture_t *tex;
    uint32_t width;
    uint32_t height;
};

struct cam_morph_data {
    obs_source_t *context;
    gs_effect_t *effect;
    gs_texture_t *dummy_tex;

    // Effect parameters
    gs_eparam_t *param_avatar_tex;
    gs_eparam_t *param_bg_tex;
    gs_eparam_t *param_out_size;
    gs_eparam_t *param_src_size;
    gs_eparam_t *param_pad_offset;
    gs_eparam_t *param_morph_t;
    gs_eparam_t *param_crop;
    gs_eparam_t *param_zoom;
    gs_eparam_t *param_pan;
    gs_eparam_t *param_corner_radius;
    gs_eparam_t *param_border_width;
    gs_eparam_t *param_border_color;
    gs_eparam_t *param_glow_size;
    gs_eparam_t *param_glow_intensity;
    gs_eparam_t *param_glow_color;
    gs_eparam_t *param_shadow_blur;
    gs_eparam_t *param_shadow_offset;
    gs_eparam_t *param_shadow_color;
    gs_eparam_t *param_has_avatar;
    gs_eparam_t *param_avatar_tex_size;
    gs_eparam_t *param_avatar_scale;
    gs_eparam_t *param_avatar_offset;
    gs_eparam_t *param_anim_breath;
    gs_eparam_t *param_anim_bounce;
    gs_eparam_t *param_anim_sway;
    gs_eparam_t *param_bg_mode;
    gs_eparam_t *param_bg_color;
    gs_eparam_t *param_bg_blur;

    // Mode & Hotkey
    int mode;
    float transition_speed;
    float morph_t;
    obs_hotkey_id hotkey_toggle;

    // Webcam Geometry
    float corner_radius;
    float zoom;
    float pan_x;
    float pan_y;
    float crop_left;
    float crop_right;
    float crop_top;
    float crop_bottom;

    // Border & Glow
    float border_width;
    struct vec4 border_color;
    float glow_size;
    float glow_intensity;
    struct vec4 glow_color;

    // Shadow
    bool shadow_enable;
    float shadow_blur;
    float shadow_offset_x;
    float shadow_offset_y;
    struct vec4 shadow_color;

    // Audio Capture
    char *audio_source_name;
    obs_source_t *audio_source;
    volatile float current_db;
    float audio_threshold_db;
    float talk_hold_sec;
    float talk_hold_timer;
    bool is_talking;
    bool was_talking;

    // PNGTuber Images
    struct tex_slot img_idle;
    struct tex_slot img_talk;
    struct tex_slot img_blink;
    struct tex_slot img_talk_blink;
    struct tex_slot img_bg;

    // Blink & Animations
    float blink_interval;
    float blink_duration;
    float blink_timer;
    bool is_blinking;

    float avatar_scale;
    float avatar_offset_x;
    float avatar_offset_y;

    float anim_breath_amp;
    float anim_bounce_amp;
    float anim_sway_amp;

    float time_acc;
    float bounce_phase;
    bool bounce_active;

    // Background
    int bg_mode;
    struct vec4 bg_color;
    float bg_blur;

    // Calculated dimensions
    uint32_t out_width;
    uint32_t out_height;
    float pad_left;
    float pad_top;
};

obs_source_t *cam_morph_get_source(struct cam_morph_data *filter)
{
    return filter ? filter->context : NULL;
}

float cam_morph_get_current_db(struct cam_morph_data *filter)
{
    return filter ? filter->current_db : -100.0f;
}

bool cam_morph_is_talking(struct cam_morph_data *filter)
{
    return filter ? filter->is_talking : false;
}

static const char *cam_morph_get_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return obs_module_text("CamMorphFilter");
}

static void audio_capture_cb(void *param, obs_source_t *source,
                             const struct audio_data *audio_data, bool muted)
{
    UNUSED_PARAMETER(source);
    struct cam_morph_data *filter = param;

    if (muted || !audio_data || audio_data->frames == 0 || !audio_data->data[0]) {
        filter->current_db = -100.0f;
        return;
    }

    const float *samples = (const float *)audio_data->data[0];
    double sum_sq = 0.0;
    for (uint32_t i = 0; i < audio_data->frames; i++) {
        float s = samples[i];
        sum_sq += (double)s * (double)s;
    }
    float rms = (float)sqrt(sum_sq / (double)audio_data->frames);
    float db = (rms > 1e-6f) ? (20.0f * log10f(rms)) : -100.0f;
    filter->current_db = db;
}

static void detach_audio_source(struct cam_morph_data *filter)
{
    if (filter->audio_source) {
        obs_source_remove_audio_capture_callback(filter->audio_source, audio_capture_cb, filter);
        obs_source_release(filter->audio_source);
        filter->audio_source = NULL;
    }
    filter->current_db = -100.0f;
}

static void attach_audio_source(struct cam_morph_data *filter, const char *name)
{
    if (filter->audio_source_name && name && strcmp(filter->audio_source_name, name) == 0 && filter->audio_source) {
        return;
    }

    detach_audio_source(filter);
    bfree(filter->audio_source_name);
    filter->audio_source_name = (name && *name) ? bstrdup(name) : NULL;

    if (filter->audio_source_name) {
        obs_source_t *src = obs_get_source_by_name(filter->audio_source_name);
        if (src) {
            obs_source_add_audio_capture_callback(src, audio_capture_cb, filter);
            filter->audio_source = src;
        }
    }
}

static void free_tex_slot(struct tex_slot *slot)
{
    if (slot->tex) {
        obs_enter_graphics();
        gs_texture_destroy(slot->tex);
        obs_leave_graphics();
        slot->tex = NULL;
    }
    bfree(slot->path);
    slot->path = NULL;
    slot->width = 0;
    slot->height = 0;
}

static void reload_tex_slot(struct tex_slot *slot, const char *new_path)
{
    const char *cur = slot->path ? slot->path : "";
    const char *nxt = new_path ? new_path : "";
    if (strcmp(cur, nxt) == 0) {
        return;
    }

    free_tex_slot(slot);

    if (nxt[0] != '\0') {
        slot->path = bstrdup(nxt);
        obs_enter_graphics();
        slot->tex = gs_texture_create_from_file(slot->path);
        if (slot->tex) {
            slot->width = gs_texture_get_width(slot->tex);
            slot->height = gs_texture_get_height(slot->tex);
        } else {
            blog(LOG_WARNING, "[obs-cam-morph] Failed to load image: %s", slot->path);
        }
        obs_leave_graphics();
    }
}

static void hotkey_toggle_cb(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
    UNUSED_PARAMETER(id);
    UNUSED_PARAMETER(hotkey);
    if (!pressed) return;

    struct cam_morph_data *filter = data;
    filter->mode = (filter->mode == MODE_WEBCAM) ? MODE_PNGTUBER : MODE_WEBCAM;

    obs_data_t *settings = obs_source_get_settings(filter->context);
    if (settings) {
        obs_data_set_int(settings, "mode", filter->mode);
        obs_data_release(settings);
    }
}

static void cam_morph_update(void *data, obs_data_t *settings)
{
    struct cam_morph_data *filter = data;

    filter->mode = (int)obs_data_get_int(settings, "mode");
    filter->transition_speed = (float)obs_data_get_double(settings, "transition_speed");

    filter->corner_radius = (float)obs_data_get_double(settings, "corner_radius");
    filter->zoom = (float)obs_data_get_double(settings, "zoom");
    filter->pan_x = (float)obs_data_get_double(settings, "pan_x");
    filter->pan_y = (float)obs_data_get_double(settings, "pan_y");

    filter->crop_left = (float)obs_data_get_double(settings, "crop_left") / 100.0f;
    filter->crop_right = (float)obs_data_get_double(settings, "crop_right") / 100.0f;
    filter->crop_top = (float)obs_data_get_double(settings, "crop_top") / 100.0f;
    filter->crop_bottom = (float)obs_data_get_double(settings, "crop_bottom") / 100.0f;
    if (filter->crop_left + filter->crop_right > 0.95f) filter->crop_right = 0.95f - filter->crop_left;
    if (filter->crop_top + filter->crop_bottom > 0.95f) filter->crop_bottom = 0.95f - filter->crop_top;

    filter->border_width = (float)obs_data_get_double(settings, "border_width");
    vec4_from_rgba(&filter->border_color, (uint32_t)obs_data_get_int(settings, "border_color"));
    filter->glow_size = (float)obs_data_get_double(settings, "glow_size");
    filter->glow_intensity = (float)obs_data_get_double(settings, "glow_intensity");
    vec4_from_rgba(&filter->glow_color, (uint32_t)obs_data_get_int(settings, "glow_color"));

    filter->shadow_enable = obs_data_get_bool(settings, "shadow_enable");
    filter->shadow_blur = (float)obs_data_get_double(settings, "shadow_blur");
    filter->shadow_offset_x = (float)obs_data_get_double(settings, "shadow_offset_x");
    filter->shadow_offset_y = (float)obs_data_get_double(settings, "shadow_offset_y");
    vec4_from_rgba(&filter->shadow_color, (uint32_t)obs_data_get_int(settings, "shadow_color"));
    if (!filter->shadow_enable) filter->shadow_color.w = 0.0f;

    attach_audio_source(filter, obs_data_get_string(settings, "audio_source"));
    filter->audio_threshold_db = (float)obs_data_get_double(settings, "audio_threshold");
    filter->talk_hold_sec = (float)obs_data_get_double(settings, "talk_hold_ms") / 1000.0f;

    reload_tex_slot(&filter->img_idle, obs_data_get_string(settings, "img_idle"));
    reload_tex_slot(&filter->img_talk, obs_data_get_string(settings, "img_talk"));
    reload_tex_slot(&filter->img_blink, obs_data_get_string(settings, "img_blink"));
    reload_tex_slot(&filter->img_talk_blink, obs_data_get_string(settings, "img_talk_blink"));
    reload_tex_slot(&filter->img_bg, obs_data_get_string(settings, "bg_image_file"));

    filter->blink_interval = (float)obs_data_get_double(settings, "blink_interval");
    filter->blink_duration = (float)obs_data_get_double(settings, "blink_duration") / 1000.0f;
    filter->avatar_scale = (float)obs_data_get_double(settings, "avatar_scale");
    filter->avatar_offset_x = (float)obs_data_get_double(settings, "avatar_offset_x");
    filter->avatar_offset_y = (float)obs_data_get_double(settings, "avatar_offset_y");

    filter->anim_breath_amp = (float)obs_data_get_double(settings, "anim_breath");
    filter->anim_bounce_amp = (float)obs_data_get_double(settings, "anim_bounce");
    filter->anim_sway_amp = (float)obs_data_get_double(settings, "anim_sway");

    filter->bg_mode = (int)obs_data_get_int(settings, "bg_mode");
    vec4_from_rgba(&filter->bg_color, (uint32_t)obs_data_get_int(settings, "bg_color"));
    filter->bg_blur = (float)obs_data_get_double(settings, "bg_blur") / 1000.0f;
}

static void *cam_morph_create(obs_data_t *settings, obs_source_t *context)
{
    struct cam_morph_data *filter = bzalloc(sizeof(struct cam_morph_data));
    filter->context = context;
    filter->current_db = -100.0f;
    filter->blink_timer = 3.0f;
    filter->hotkey_toggle = OBS_INVALID_HOTKEY_ID;

    char *effect_path = obs_module_file("effects/cam_morph.effect");
    obs_enter_graphics();
    uint32_t zero_px = 0;
    const uint8_t *px_ptr = (const uint8_t *)&zero_px;
    filter->dummy_tex = gs_texture_create(1, 1, GS_RGBA, 1, &px_ptr, 0);

    filter->effect = gs_effect_create_from_file(effect_path, NULL);
    if (filter->effect) {
        filter->param_avatar_tex      = gs_effect_get_param_by_name(filter->effect, "avatar_tex");
        filter->param_bg_tex          = gs_effect_get_param_by_name(filter->effect, "bg_tex");
        filter->param_out_size        = gs_effect_get_param_by_name(filter->effect, "out_size");
        filter->param_src_size        = gs_effect_get_param_by_name(filter->effect, "src_size");
        filter->param_pad_offset      = gs_effect_get_param_by_name(filter->effect, "pad_offset");
        filter->param_morph_t         = gs_effect_get_param_by_name(filter->effect, "morph_t");
        filter->param_crop            = gs_effect_get_param_by_name(filter->effect, "crop");
        filter->param_zoom            = gs_effect_get_param_by_name(filter->effect, "zoom");
        filter->param_pan             = gs_effect_get_param_by_name(filter->effect, "pan");
        filter->param_corner_radius   = gs_effect_get_param_by_name(filter->effect, "corner_radius");
        filter->param_border_width    = gs_effect_get_param_by_name(filter->effect, "border_width");
        filter->param_border_color    = gs_effect_get_param_by_name(filter->effect, "border_color");
        filter->param_glow_size       = gs_effect_get_param_by_name(filter->effect, "glow_size");
        filter->param_glow_intensity  = gs_effect_get_param_by_name(filter->effect, "glow_intensity");
        filter->param_glow_color      = gs_effect_get_param_by_name(filter->effect, "glow_color");
        filter->param_shadow_blur     = gs_effect_get_param_by_name(filter->effect, "shadow_blur");
        filter->param_shadow_offset   = gs_effect_get_param_by_name(filter->effect, "shadow_offset");
        filter->param_shadow_color    = gs_effect_get_param_by_name(filter->effect, "shadow_color");
        filter->param_has_avatar      = gs_effect_get_param_by_name(filter->effect, "has_avatar");
        filter->param_avatar_tex_size = gs_effect_get_param_by_name(filter->effect, "avatar_tex_size");
        filter->param_avatar_scale    = gs_effect_get_param_by_name(filter->effect, "avatar_scale");
        filter->param_avatar_offset   = gs_effect_get_param_by_name(filter->effect, "avatar_offset");
        filter->param_anim_breath     = gs_effect_get_param_by_name(filter->effect, "anim_breath");
        filter->param_anim_bounce     = gs_effect_get_param_by_name(filter->effect, "anim_bounce");
        filter->param_anim_sway       = gs_effect_get_param_by_name(filter->effect, "anim_sway");
        filter->param_bg_mode         = gs_effect_get_param_by_name(filter->effect, "bg_mode");
        filter->param_bg_color        = gs_effect_get_param_by_name(filter->effect, "bg_color");
        filter->param_bg_blur         = gs_effect_get_param_by_name(filter->effect, "bg_blur");
    }
    obs_leave_graphics();
    bfree(effect_path);

    filter->hotkey_toggle = obs_hotkey_register_source(
        context, "cam_morph_toggle_mode", obs_module_text("HotkeyToggle"),
        hotkey_toggle_cb, filter);

    cam_morph_update(filter, settings);
    filter->morph_t = (filter->mode == MODE_PNGTUBER) ? 1.0f : 0.0f;
    return filter;
}

static void cam_morph_destroy(void *data)
{
    struct cam_morph_data *filter = data;
    cam_morph_ui_close_for_filter(filter);

    if (filter->hotkey_toggle != OBS_INVALID_HOTKEY_ID) {
        obs_hotkey_unregister(filter->hotkey_toggle);
    }
    detach_audio_source(filter);
    bfree(filter->audio_source_name);

    free_tex_slot(&filter->img_idle);
    free_tex_slot(&filter->img_talk);
    free_tex_slot(&filter->img_blink);
    free_tex_slot(&filter->img_talk_blink);
    free_tex_slot(&filter->img_bg);

    obs_enter_graphics();
    if (filter->dummy_tex) gs_texture_destroy(filter->dummy_tex);
    if (filter->effect) gs_effect_destroy(filter->effect);
    obs_leave_graphics();

    bfree(filter);
}

static void cam_morph_video_tick(void *data, float seconds)
{
    struct cam_morph_data *filter = data;
    filter->time_acc += seconds;

    float target_t = (filter->mode == MODE_PNGTUBER) ? 1.0f : 0.0f;
    if (filter->transition_speed <= 0.01f) {
        filter->morph_t = target_t;
    } else {
        float step = seconds / filter->transition_speed;
        if (filter->morph_t < target_t) {
            filter->morph_t = fminf(filter->morph_t + step, target_t);
        } else if (filter->morph_t > target_t) {
            filter->morph_t = fmaxf(filter->morph_t - step, target_t);
        }
    }

    float db = filter->current_db;
    if (db >= filter->audio_threshold_db) {
        filter->is_talking = true;
        filter->talk_hold_timer = filter->talk_hold_sec;
    } else if (filter->talk_hold_timer > 0.0f) {
        filter->talk_hold_timer -= seconds;
        if (filter->talk_hold_timer <= 0.0f) {
            filter->is_talking = false;
        }
    } else {
        filter->is_talking = false;
    }

    if (filter->is_talking && !filter->was_talking) {
        filter->bounce_active = true;
        filter->bounce_phase = 0.0f;
    }
    filter->was_talking = filter->is_talking;

    if (filter->bounce_active) {
        filter->bounce_phase += seconds * 6.5f;
        if (filter->bounce_phase >= 1.0f) {
            filter->bounce_phase = 0.0f;
            filter->bounce_active = false;
        }
    }

    filter->blink_timer -= seconds;
    if (filter->blink_timer <= 0.0f) {
        if (!filter->is_blinking) {
            filter->is_blinking = true;
            filter->blink_timer = fmaxf(filter->blink_duration, 0.04f);
        } else {
            filter->is_blinking = false;
            float rnd = (float)rand() / (float)RAND_MAX;
            float base = fmaxf(filter->blink_interval, 0.5f);
            filter->blink_timer = base * (0.65f + rnd * 0.7f);
        }
    }
}

static void cam_morph_calc_dimensions(struct cam_morph_data *filter, uint32_t *base_w, uint32_t *base_h)
{
    obs_source_t *target = obs_filter_get_target(filter->context);
    if (!target) {
        *base_w = 0;
        *base_h = 0;
        filter->out_width = 0;
        filter->out_height = 0;
        return;
    }

    *base_w = obs_source_get_base_width(target);
    *base_h = obs_source_get_base_height(target);

    float vis_w = (float)(*base_w) * (1.0f - filter->crop_left - filter->crop_right);
    float vis_h = (float)(*base_h) * (1.0f - filter->crop_top - filter->crop_bottom);
    if (vis_w < 2.0f) vis_w = 2.0f;
    if (vis_h < 2.0f) vis_h = 2.0f;

    float extra = filter->border_width + filter->glow_size;
    float shadow_pad_l = filter->shadow_enable ? fmaxf(0.0f, filter->shadow_blur - filter->shadow_offset_x) : 0.0f;
    float shadow_pad_r = filter->shadow_enable ? fmaxf(0.0f, filter->shadow_blur + filter->shadow_offset_x) : 0.0f;
    float shadow_pad_t = filter->shadow_enable ? fmaxf(0.0f, filter->shadow_blur - filter->shadow_offset_y) : 0.0f;
    float shadow_pad_b = filter->shadow_enable ? fmaxf(0.0f, filter->shadow_blur + filter->shadow_offset_y) : 0.0f;

    float pad_l = ceilf(fmaxf(extra, shadow_pad_l)) + 2.0f;
    float pad_r = ceilf(fmaxf(extra, shadow_pad_r)) + 2.0f;
    float pad_t = ceilf(fmaxf(extra, shadow_pad_t)) + 2.0f;
    float pad_b = ceilf(fmaxf(extra, shadow_pad_b)) + 2.0f;

    filter->pad_left = pad_l;
    filter->pad_top = pad_t;
    filter->out_width = (uint32_t)ceilf(vis_w + pad_l + pad_r);
    filter->out_height = (uint32_t)ceilf(vis_h + pad_t + pad_b);
}

static uint32_t cam_morph_get_width(void *data)
{
    struct cam_morph_data *filter = data;
    uint32_t bw, bh;
    cam_morph_calc_dimensions(filter, &bw, &bh);
    return filter->out_width;
}

static uint32_t cam_morph_get_height(void *data)
{
    struct cam_morph_data *filter = data;
    uint32_t bw, bh;
    cam_morph_calc_dimensions(filter, &bw, &bh);
    return filter->out_height;
}

static struct tex_slot *pick_active_avatar(struct cam_morph_data *filter)
{
    if (filter->is_talking && filter->is_blinking) {
        if (filter->img_talk_blink.tex) return &filter->img_talk_blink;
        if (filter->img_talk.tex) return &filter->img_talk;
    } else if (filter->is_talking) {
        if (filter->img_talk.tex) return &filter->img_talk;
    } else if (filter->is_blinking) {
        if (filter->img_blink.tex) return &filter->img_blink;
    }
    if (filter->img_idle.tex) return &filter->img_idle;
    return NULL;
}

static void cam_morph_video_render(void *data, gs_effect_t *effect)
{
    UNUSED_PARAMETER(effect);
    struct cam_morph_data *filter = data;

    if (!filter->effect) {
        obs_source_skip_video_filter(filter->context);
        return;
    }

    uint32_t base_w = 0, base_h = 0;
    cam_morph_calc_dimensions(filter, &base_w, &base_h);
    if (base_w == 0 || base_h == 0) {
        obs_source_skip_video_filter(filter->context);
        return;
    }

    if (!obs_source_process_filter_begin(filter->context, GS_RGBA, OBS_NO_DIRECT_RENDERING)) {
        return;
    }

    struct tex_slot *av = pick_active_avatar(filter);
    gs_texture_t *av_tex = (av && av->tex) ? av->tex : filter->dummy_tex;
    struct vec2 av_tex_size = {
        av ? (float)av->width : 1.0f,
        av ? (float)av->height : 1.0f
    };
    gs_texture_t *bg_tex = filter->img_bg.tex ? filter->img_bg.tex : filter->dummy_tex;

    float breath = sinf(filter->time_acc * 2.2f) * filter->anim_breath_amp;
    float sway = sinf(filter->time_acc * 1.4f) * filter->anim_sway_amp * 0.0174533f;
    float bounce = filter->bounce_active
                       ? (sinf(filter->bounce_phase * 3.1415926f) * filter->anim_bounce_amp)
                       : 0.0f;

    struct vec2 out_size = {(float)filter->out_width, (float)filter->out_height};
    struct vec2 src_size = {(float)base_w, (float)base_h};
    struct vec2 pad_offset = {filter->pad_left, filter->pad_top};
    struct vec4 crop = {filter->crop_left, filter->crop_top, filter->crop_right, filter->crop_bottom};
    struct vec2 pan = {filter->pan_x, filter->pan_y};
    struct vec2 shadow_offset = {filter->shadow_offset_x, filter->shadow_offset_y};
    struct vec2 av_offset = {filter->avatar_offset_x, filter->avatar_offset_y};

    gs_effect_set_texture(filter->param_avatar_tex, av_tex);
    gs_effect_set_texture(filter->param_bg_tex, bg_tex);
    gs_effect_set_vec2(filter->param_out_size, &out_size);
    gs_effect_set_vec2(filter->param_src_size, &src_size);
    gs_effect_set_vec2(filter->param_pad_offset, &pad_offset);
    gs_effect_set_float(filter->param_morph_t, filter->morph_t);
    gs_effect_set_vec4(filter->param_crop, &crop);
    gs_effect_set_float(filter->param_zoom, filter->zoom);
    gs_effect_set_vec2(filter->param_pan, &pan);
    gs_effect_set_float(filter->param_corner_radius, filter->corner_radius);
    gs_effect_set_float(filter->param_border_width, filter->border_width);
    gs_effect_set_vec4(filter->param_border_color, &filter->border_color);
    gs_effect_set_float(filter->param_glow_size, filter->glow_size);
    gs_effect_set_float(filter->param_glow_intensity, filter->glow_intensity);
    gs_effect_set_vec4(filter->param_glow_color, &filter->glow_color);
    gs_effect_set_float(filter->param_shadow_blur, filter->shadow_blur);
    gs_effect_set_vec2(filter->param_shadow_offset, &shadow_offset);
    gs_effect_set_vec4(filter->param_shadow_color, &filter->shadow_color);

    gs_effect_set_int(filter->param_has_avatar, av ? 1 : 0);
    gs_effect_set_vec2(filter->param_avatar_tex_size, &av_tex_size);
    gs_effect_set_float(filter->param_avatar_scale, filter->avatar_scale);
    gs_effect_set_vec2(filter->param_avatar_offset, &av_offset);
    gs_effect_set_float(filter->param_anim_breath, breath);
    gs_effect_set_float(filter->param_anim_bounce, bounce);
    gs_effect_set_float(filter->param_anim_sway, sway);
    gs_effect_set_int(filter->param_bg_mode, filter->bg_mode);
    gs_effect_set_vec4(filter->param_bg_color, &filter->bg_color);
    gs_effect_set_float(filter->param_bg_blur, filter->bg_blur);

    gs_blend_state_push();
    gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

    obs_source_process_filter_end(filter->context, filter->effect, filter->out_width, filter->out_height);

    gs_blend_state_pop();
}

static void cam_morph_get_defaults(obs_data_t *settings)
{
    obs_data_set_default_int(settings, "mode", MODE_WEBCAM);
    obs_data_set_default_double(settings, "transition_speed", 0.35);

    obs_data_set_default_double(settings, "corner_radius", 32.0);
    obs_data_set_default_double(settings, "zoom", 1.0);
    obs_data_set_default_double(settings, "pan_x", 0.0);
    obs_data_set_default_double(settings, "pan_y", 0.0);
    obs_data_set_default_double(settings, "crop_left", 0.0);
    obs_data_set_default_double(settings, "crop_right", 0.0);
    obs_data_set_default_double(settings, "crop_top", 0.0);
    obs_data_set_default_double(settings, "crop_bottom", 0.0);

    obs_data_set_default_double(settings, "border_width", 4.0);
    obs_data_set_default_int(settings, "border_color", 0xFFFFFFFF);
    obs_data_set_default_double(settings, "glow_size", 20.0);
    obs_data_set_default_double(settings, "glow_intensity", 0.8);
    obs_data_set_default_int(settings, "glow_color", 0xFFFF8800);

    obs_data_set_default_bool(settings, "shadow_enable", true);
    obs_data_set_default_double(settings, "shadow_blur", 24.0);
    obs_data_set_default_double(settings, "shadow_offset_x", 0.0);
    obs_data_set_default_double(settings, "shadow_offset_y", 12.0);
    obs_data_set_default_int(settings, "shadow_color", 0xB4000000);

    obs_data_set_default_double(settings, "audio_threshold", -38.0);
    obs_data_set_default_double(settings, "talk_hold_ms", 150.0);
    obs_data_set_default_double(settings, "blink_interval", 3.5);
    obs_data_set_default_double(settings, "blink_duration", 120.0);
    obs_data_set_default_double(settings, "avatar_scale", 0.9);
    obs_data_set_default_double(settings, "avatar_offset_x", 0.0);
    obs_data_set_default_double(settings, "avatar_offset_y", 0.0);

    obs_data_set_default_double(settings, "anim_breath", 0.02);
    obs_data_set_default_double(settings, "anim_bounce", 0.06);
    obs_data_set_default_double(settings, "anim_sway", 1.5);

    obs_data_set_default_int(settings, "bg_mode", BG_BLUR_CAM);
    obs_data_set_default_int(settings, "bg_color", 0xFF28201E);
    obs_data_set_default_double(settings, "bg_blur", 18.0);
}

static bool open_retro_ui_cb(obs_properties_t *props, obs_property_t *property, void *data)
{
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(property);
    struct cam_morph_data *filter = data;
    cam_morph_ui_open(filter);
    return false;
}

static bool mode_modified_cb(obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
    UNUSED_PARAMETER(p);
    bool is_png = (obs_data_get_int(settings, "mode") == MODE_PNGTUBER);
    obs_property_set_visible(obs_properties_get(props, "group_png"), is_png);
    obs_property_set_visible(obs_properties_get(props, "group_anim"), is_png);
    obs_property_set_visible(obs_properties_get(props, "group_bg"), is_png);
    return true;
}

static bool bg_mode_modified_cb(obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
    UNUSED_PARAMETER(p);
    int bg = (int)obs_data_get_int(settings, "bg_mode");
    obs_property_set_visible(obs_properties_get(props, "bg_color"), bg == BG_COLOR);
    obs_property_set_visible(obs_properties_get(props, "bg_image_file"), bg == BG_IMAGE);
    obs_property_set_visible(obs_properties_get(props, "bg_blur"), bg == BG_BLUR_CAM);
    return true;
}

static bool enum_audio_sources_cb(void *param, obs_source_t *src)
{
    obs_property_t *list = param;
    uint32_t flags = obs_source_get_output_flags(src);
    if ((flags & OBS_SOURCE_AUDIO) != 0) {
        const char *name = obs_source_get_name(src);
        obs_property_list_add_string(list, name, name);
    }
    return true;
}

static obs_properties_t *cam_morph_get_properties(void *data)
{
    obs_properties_t *props = obs_properties_create();

    // Big button to open the Classic Macintosh Retro Panel
    obs_properties_add_button2(props, "open_retro_ui", obs_module_text("OpenRetroPanel"),
                               open_retro_ui_cb, data);

    // Smart native mode selector
    obs_property_t *mode_list = obs_properties_add_list(
        props, "mode", obs_module_text("Mode"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(mode_list, obs_module_text("ModeWebcam"), MODE_WEBCAM);
    obs_property_list_add_int(mode_list, obs_module_text("ModePNGTuber"), MODE_PNGTUBER);
    obs_property_set_modified_callback(mode_list, mode_modified_cb);

    obs_properties_add_float_slider(props, "transition_speed", obs_module_text("TransitionSpeed"), 0.0, 2.0, 0.05);

    // Group 1: Geometry
    obs_properties_t *geom = obs_properties_create();
    obs_properties_add_float_slider(geom, "corner_radius", obs_module_text("CornerRadius"), 0.0, 500.0, 1.0);
    obs_properties_add_float_slider(geom, "zoom", obs_module_text("Zoom"), 1.0, 4.0, 0.01);
    obs_properties_add_float_slider(geom, "pan_x", obs_module_text("PanX"), -1.0, 1.0, 0.01);
    obs_properties_add_float_slider(geom, "pan_y", obs_module_text("PanY"), -1.0, 1.0, 0.01);
    obs_properties_add_float_slider(geom, "crop_left", obs_module_text("CropLeft"), 0.0, 45.0, 0.5);
    obs_properties_add_float_slider(geom, "crop_right", obs_module_text("CropRight"), 0.0, 45.0, 0.5);
    obs_properties_add_float_slider(geom, "crop_top", obs_module_text("CropTop"), 0.0, 45.0, 0.5);
    obs_properties_add_float_slider(geom, "crop_bottom", obs_module_text("CropBottom"), 0.0, 45.0, 0.5);
    obs_properties_add_group(props, "group_geom", obs_module_text("GroupGeometry"), OBS_GROUP_NORMAL, geom);

    // Group 2: Border & Glow
    obs_properties_t *border = obs_properties_create();
    obs_properties_add_float_slider(border, "border_width", obs_module_text("BorderWidth"), 0.0, 40.0, 0.5);
    obs_properties_add_color_alpha(border, "border_color", obs_module_text("BorderColor"));
    obs_properties_add_float_slider(border, "glow_size", obs_module_text("GlowSize"), 0.0, 100.0, 1.0);
    obs_properties_add_float_slider(border, "glow_intensity", obs_module_text("GlowIntensity"), 0.0, 2.0, 0.05);
    obs_properties_add_color_alpha(border, "glow_color", obs_module_text("GlowColor"));
    obs_properties_add_group(props, "group_border", obs_module_text("GroupBorder"), OBS_GROUP_NORMAL, border);

    // Group 3: Drop Shadow
    obs_properties_t *shadow = obs_properties_create();
    obs_properties_add_bool(shadow, "shadow_enable", obs_module_text("ShadowEnable"));
    obs_properties_add_float_slider(shadow, "shadow_blur", obs_module_text("ShadowBlur"), 0.0, 100.0, 1.0);
    obs_properties_add_float_slider(shadow, "shadow_offset_x", obs_module_text("ShadowOffsetX"), -100.0, 100.0, 1.0);
    obs_properties_add_float_slider(shadow, "shadow_offset_y", obs_module_text("ShadowOffsetY"), -100.0, 100.0, 1.0);
    obs_properties_add_color_alpha(shadow, "shadow_color", obs_module_text("ShadowColor"));
    obs_properties_add_group(props, "group_shadow", obs_module_text("GroupShadow"), OBS_GROUP_NORMAL, shadow);

    // Group 4: PNGTuber Model & Mic
    obs_properties_t *png = obs_properties_create();
    obs_property_t *audio_list = obs_properties_add_list(
        png, "audio_source", obs_module_text("AudioSource"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(audio_list, obs_module_text("AudioSourceNone"), "");
    obs_enum_sources(enum_audio_sources_cb, audio_list);

    obs_properties_add_float_slider(png, "audio_threshold", obs_module_text("AudioThreshold"), -80.0, 0.0, 0.5);
    obs_properties_add_float_slider(png, "talk_hold_ms", obs_module_text("TalkHoldMs"), 0.0, 600.0, 10.0);

    const char *img_filter = "Images (*.png *.jpg *.jpeg *.webp *.bmp);;All Files (*.*)";
    obs_properties_add_path(png, "img_idle", obs_module_text("ImgIdle"), OBS_PATH_FILE, img_filter, NULL);
    obs_properties_add_path(png, "img_talk", obs_module_text("ImgTalk"), OBS_PATH_FILE, img_filter, NULL);
    obs_properties_add_path(png, "img_blink", obs_module_text("ImgBlink"), OBS_PATH_FILE, img_filter, NULL);
    obs_properties_add_path(png, "img_talk_blink", obs_module_text("ImgTalkBlink"), OBS_PATH_FILE, img_filter, NULL);

    obs_properties_add_float_slider(png, "blink_interval", obs_module_text("BlinkInterval"), 0.5, 10.0, 0.1);
    obs_properties_add_float_slider(png, "blink_duration", obs_module_text("BlinkDuration"), 40.0, 400.0, 10.0);
    obs_properties_add_float_slider(png, "avatar_scale", obs_module_text("AvatarScale"), 0.2, 2.5, 0.01);
    obs_properties_add_float_slider(png, "avatar_offset_x", obs_module_text("AvatarOffsetX"), -0.5, 0.5, 0.01);
    obs_properties_add_float_slider(png, "avatar_offset_y", obs_module_text("AvatarOffsetY"), -0.5, 0.5, 0.01);
    obs_properties_add_group(props, "group_png", obs_module_text("GroupPNGTuber"), OBS_GROUP_NORMAL, png);

    // Group 5: PNGTuber Animations
    obs_properties_t *anim = obs_properties_create();
    obs_properties_add_float_slider(anim, "anim_breath", obs_module_text("AnimBreath"), 0.0, 0.12, 0.005);
    obs_properties_add_float_slider(anim, "anim_bounce", obs_module_text("AnimBounce"), 0.0, 0.25, 0.005);
    obs_properties_add_float_slider(anim, "anim_sway", obs_module_text("AnimSway"), 0.0, 10.0, 0.2);
    obs_properties_add_group(props, "group_anim", obs_module_text("GroupAnim"), OBS_GROUP_NORMAL, anim);

    // Group 6: PNGTuber Background
    obs_properties_t *bg = obs_properties_create();
    obs_property_t *bg_list = obs_properties_add_list(
        bg, "bg_mode", obs_module_text("BgMode"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(bg_list, obs_module_text("BgColor"), BG_COLOR);
    obs_property_list_add_int(bg_list, obs_module_text("BgImage"), BG_IMAGE);
    obs_property_list_add_int(bg_list, obs_module_text("BgBlurCam"), BG_BLUR_CAM);
    obs_property_list_add_int(bg_list, obs_module_text("BgTransparent"), BG_TRANSPARENT);
    obs_property_set_modified_callback(bg_list, bg_mode_modified_cb);

    obs_properties_add_color_alpha(bg, "bg_color", obs_module_text("BgColorPick"));
    obs_properties_add_path(bg, "bg_image_file", obs_module_text("BgImageFile"), OBS_PATH_FILE, img_filter, NULL);
    obs_properties_add_float_slider(bg, "bg_blur", obs_module_text("BgBlurAmount"), 1.0, 50.0, 1.0);
    obs_properties_add_group(props, "group_bg", obs_module_text("GroupBG"), OBS_GROUP_NORMAL, bg);

    return props;
}

struct obs_source_info cam_morph_filter_info = {
    .id             = "cam_morph_filter",
    .type           = OBS_SOURCE_TYPE_FILTER,
    .output_flags   = OBS_SOURCE_VIDEO,
    .get_name       = cam_morph_get_name,
    .create         = cam_morph_create,
    .destroy        = cam_morph_destroy,
    .update         = cam_morph_update,
    .get_defaults   = cam_morph_get_defaults,
    .get_properties = cam_morph_get_properties,
    .video_tick     = cam_morph_video_tick,
    .video_render   = cam_morph_video_render,
    .get_width      = cam_morph_get_width,
    .get_height     = cam_morph_get_height,
};

bool obs_module_load(void)
{
    obs_register_source(&cam_morph_filter_info);
    blog(LOG_INFO, "[obs-cam-morph] Plugin loaded successfully (v0.3.0 Macintosh Platinum UI)");
    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "[obs-cam-morph] Plugin unloaded");
}
