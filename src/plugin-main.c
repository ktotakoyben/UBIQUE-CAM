#include "ubique-common.h"
#include <graphics/graphics.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("ubique-cam", "ru-RU")

#define SPEC_BANDS 32
#define GOERTZEL_N 256

struct tex_slot {
    char *path;
    gs_texture_t *tex;
    uint32_t width;
    uint32_t height;
};

struct ubique_cam_data {
    obs_source_t *context;
    gs_effect_t *effect;
    gs_texture_t *dummy_tex;
    gs_texture_t *spec_tex;
    uint8_t spec_rgba[SPEC_BANDS * 4];

    // Effect parameters
    gs_eparam_t *param_avatar_tex;
    gs_eparam_t *param_bg_tex;
    gs_eparam_t *param_spec_tex;
    gs_eparam_t *param_out_size;
    gs_eparam_t *param_src_size;
    gs_eparam_t *param_pad_offset;
    gs_eparam_t *param_time;

    gs_eparam_t *param_morph_t;
    gs_eparam_t *param_trans_type;

    gs_eparam_t *param_crop;
    gs_eparam_t *param_zoom;
    gs_eparam_t *param_pan;
    gs_eparam_t *param_flip_h;
    gs_eparam_t *param_vignette;
    gs_eparam_t *param_contrast;
    gs_eparam_t *param_warmth;

    gs_eparam_t *param_corner_radius;
    gs_eparam_t *param_border_width;
    gs_eparam_t *param_border_style;
    gs_eparam_t *param_border_grad_speed;
    gs_eparam_t *param_border_color;
    gs_eparam_t *param_border_color2;

    gs_eparam_t *param_glow_size;
    gs_eparam_t *param_glow_intensity;
    gs_eparam_t *param_glow_color;
    gs_eparam_t *param_audio_level;
    gs_eparam_t *param_peak_level;
    gs_eparam_t *param_peak_color;

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
    gs_eparam_t *param_shake_offset;
    gs_eparam_t *param_idle_dim_factor;

    gs_eparam_t *param_bg_mode;
    gs_eparam_t *param_bg_color;
    gs_eparam_t *param_bg_blur;

    gs_eparam_t *param_spec_layer;
    gs_eparam_t *param_spec_design;
    gs_eparam_t *param_spec_color;
    gs_eparam_t *param_spec_height;
    gs_eparam_t *param_spec_opacity;

    // Mode & Hotkey
    int mode;
    int transition_type;
    float transition_speed;
    float morph_t;
    obs_hotkey_id hotkey_toggle;

    // Webcam Geometry & Optics
    float corner_radius;
    float zoom;
    float pan_x;
    float pan_y;
    float crop_left;
    float crop_right;
    float crop_top;
    float crop_bottom;
    bool flip_h;
    float vignette;
    float contrast;
    float warmth;

    // Border & Glow & Audio Reactivity
    float border_width;
    int border_style;
    float border_grad_speed;
    struct vec4 border_color;
    struct vec4 border_color2;

    float glow_size;
    float glow_intensity;
    struct vec4 glow_color;
    bool border_audio_react;
    float border_react_strength;
    float peak_threshold_db;
    struct vec4 peak_color;
    float smooth_audio_level;
    float smooth_peak_level;

    // Shadow
    bool shadow_enable;
    float shadow_blur;
    float shadow_offset_x;
    float shadow_offset_y;
    struct vec4 shadow_color;

    // Mic Audio Capture
    char *audio_source_name;
    obs_source_t *audio_source;
    volatile float current_db;
    float audio_threshold_db;
    float talk_hold_sec;
    float talk_hold_timer;
    float silent_timer;
    bool is_talking;
    bool was_talking;

    // Spectrum Audio Capture & Goertzel Bands
    char *spec_source_name;
    obs_source_t *spec_source;
    volatile float raw_bands[SPEC_BANDS];
    float smooth_bands[SPEC_BANDS];
    float goertzel_coeff[SPEC_BANDS];
    int spec_layer;
    int spec_design;
    struct vec4 spec_color;
    float spec_sensitivity;
    float spec_height;
    float spec_opacity;

    // PNGTuber Images
    struct tex_slot img_idle;
    struct tex_slot img_talk;
    struct tex_slot img_blink;
    struct tex_slot img_talk_blink;
    struct tex_slot img_bg;

    // Blink & Animations & Scream Shake & Idle Dim
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
    float scream_threshold_db;
    float scream_shake_amp;
    float idle_dim_amp;
    float cur_idle_dim;

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

obs_source_t *ubique_get_source(struct ubique_cam_data *filter)
{
    return filter ? filter->context : NULL;
}

float ubique_get_current_db(struct ubique_cam_data *filter)
{
    return filter ? filter->current_db : -100.0f;
}

bool ubique_is_talking(struct ubique_cam_data *filter)
{
    return filter ? filter->is_talking : false;
}

static const char *ubique_get_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return obs_module_text("UbiqueCamFilter");
}

static void init_goertzel_coeffs(struct ubique_cam_data *filter)
{
    const float sample_rate = 48000.0f;
    const float f_min = 70.0f;
    const float f_max = 12000.0f;
    for (int b = 0; b < SPEC_BANDS; b++) {
        float t = (float)b / (float)(SPEC_BANDS - 1);
        float freq = f_min * powf(f_max / f_min, t);
        float omega = 2.0f * 3.14159265f * freq / sample_rate;
        filter->goertzel_coeff[b] = 2.0f * cosf(omega);
    }
}

static void compute_spectrum_bands(struct ubique_cam_data *filter, const float *samples, uint32_t frames)
{
    uint32_t n = frames < GOERTZEL_N ? frames : GOERTZEL_N;
    if (n < 16) return;

    float inv_n = 1.0f / (float)n;
    for (int b = 0; b < SPEC_BANDS; b++) {
        float coeff = filter->goertzel_coeff[b];
        float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            // Hann-like lightweight window
            float w = 0.5f * (1.0f - cosf(6.2831853f * (float)i * inv_n));
            s0 = samples[i] * w + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        float power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
        if (power < 0.0f) power = 0.0f;
        float mag = sqrtf(power) * inv_n * 4.0f;
        // Slight high-frequency boost for visual balance
        float boost = 1.0f + ((float)b / (float)SPEC_BANDS) * 1.8f;
        float val = log1pf(mag * boost * 14.0f) / 3.2f;
        if (val > 1.0f) val = 1.0f;
        filter->raw_bands[b] = val;
    }
}

static void mic_audio_cb(void *param, obs_source_t *source,
                         const struct audio_data *audio_data, bool muted)
{
    UNUSED_PARAMETER(source);
    struct ubique_cam_data *filter = param;

    if (muted || !audio_data || audio_data->frames == 0 || !audio_data->data[0]) {
        filter->current_db = -100.0f;
        if (!filter->spec_source) {
            for (int b = 0; b < SPEC_BANDS; b++) filter->raw_bands[b] = 0.0f;
        }
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

    // If no separate spectrum source is chosen, drive spectrum from mic
    if (!filter->spec_source && filter->spec_layer != SPEC_LAYER_OFF) {
        compute_spectrum_bands(filter, samples, audio_data->frames);
    }
}

static void spec_audio_cb(void *param, obs_source_t *source,
                          const struct audio_data *audio_data, bool muted)
{
    UNUSED_PARAMETER(source);
    struct ubique_cam_data *filter = param;
    if (filter->spec_layer == SPEC_LAYER_OFF) return;

    if (muted || !audio_data || audio_data->frames == 0 || !audio_data->data[0]) {
        for (int b = 0; b < SPEC_BANDS; b++) filter->raw_bands[b] = 0.0f;
        return;
    }

    const float *samples = (const float *)audio_data->data[0];
    compute_spectrum_bands(filter, samples, audio_data->frames);
}

static void detach_audio_source(struct ubique_cam_data *filter)
{
    if (filter->audio_source) {
        obs_source_remove_audio_capture_callback(filter->audio_source, mic_audio_cb, filter);
        obs_source_release(filter->audio_source);
        filter->audio_source = NULL;
    }
    filter->current_db = -100.0f;
}

static void attach_audio_source(struct ubique_cam_data *filter, const char *name)
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
            obs_source_add_audio_capture_callback(src, mic_audio_cb, filter);
            filter->audio_source = src;
        }
    }
}

static void detach_spec_source(struct ubique_cam_data *filter)
{
    if (filter->spec_source) {
        obs_source_remove_audio_capture_callback(filter->spec_source, spec_audio_cb, filter);
        obs_source_release(filter->spec_source);
        filter->spec_source = NULL;
    }
}

static void attach_spec_source(struct ubique_cam_data *filter, const char *name)
{
    if (filter->spec_source_name && name && strcmp(filter->spec_source_name, name) == 0 && filter->spec_source) {
        return;
    }
    detach_spec_source(filter);
    bfree(filter->spec_source_name);
    filter->spec_source_name = (name && *name) ? bstrdup(name) : NULL;

    if (filter->spec_source_name) {
        obs_source_t *src = obs_get_source_by_name(filter->spec_source_name);
        if (src) {
            obs_source_add_audio_capture_callback(src, spec_audio_cb, filter);
            filter->spec_source = src;
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
    if (strcmp(cur, nxt) == 0) return;

    free_tex_slot(slot);
    if (nxt[0] != '\0') {
        slot->path = bstrdup(nxt);
        obs_enter_graphics();
        slot->tex = gs_texture_create_from_file(slot->path);
        if (slot->tex) {
            slot->width = gs_texture_get_width(slot->tex);
            slot->height = gs_texture_get_height(slot->tex);
        } else {
            blog(LOG_WARNING, "[ubique-cam] Failed to load image: %s", slot->path);
        }
        obs_leave_graphics();
    }
}

static void hotkey_toggle_cb(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
    UNUSED_PARAMETER(id);
    UNUSED_PARAMETER(hotkey);
    if (!pressed) return;

    struct ubique_cam_data *filter = data;
    filter->mode = (filter->mode == MODE_WEBCAM) ? MODE_PNGTUBER : MODE_WEBCAM;

    obs_data_t *settings = obs_source_get_settings(filter->context);
    if (settings) {
        obs_data_set_int(settings, "mode", filter->mode);
        obs_data_release(settings);
    }
}

static void ubique_update(void *data, obs_data_t *settings)
{
    struct ubique_cam_data *filter = data;

    filter->mode = (int)obs_data_get_int(settings, "mode");
    filter->transition_type = (int)obs_data_get_int(settings, "transition_type");
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

    filter->flip_h = obs_data_get_bool(settings, "flip_h");
    filter->vignette = (float)obs_data_get_double(settings, "vignette");
    filter->contrast = (float)obs_data_get_double(settings, "contrast");
    filter->warmth = (float)obs_data_get_double(settings, "warmth");

    filter->border_width = (float)obs_data_get_double(settings, "border_width");
    filter->border_style = (int)obs_data_get_int(settings, "border_style");
    filter->border_grad_speed = (float)obs_data_get_double(settings, "border_grad_speed");
    vec4_from_rgba(&filter->border_color, (uint32_t)obs_data_get_int(settings, "border_color"));
    vec4_from_rgba(&filter->border_color2, (uint32_t)obs_data_get_int(settings, "border_color2"));

    filter->glow_size = (float)obs_data_get_double(settings, "glow_size");
    filter->glow_intensity = (float)obs_data_get_double(settings, "glow_intensity");
    vec4_from_rgba(&filter->glow_color, (uint32_t)obs_data_get_int(settings, "glow_color"));
    filter->border_audio_react = obs_data_get_bool(settings, "border_audio_react");
    filter->border_react_strength = (float)obs_data_get_double(settings, "border_react_strength");
    filter->peak_threshold_db = (float)obs_data_get_double(settings, "peak_threshold_db");
    vec4_from_rgba(&filter->peak_color, (uint32_t)obs_data_get_int(settings, "peak_color"));

    filter->shadow_enable = obs_data_get_bool(settings, "shadow_enable");
    filter->shadow_blur = (float)obs_data_get_double(settings, "shadow_blur");
    filter->shadow_offset_x = (float)obs_data_get_double(settings, "shadow_offset_x");
    filter->shadow_offset_y = (float)obs_data_get_double(settings, "shadow_offset_y");
    vec4_from_rgba(&filter->shadow_color, (uint32_t)obs_data_get_int(settings, "shadow_color"));
    if (!filter->shadow_enable) filter->shadow_color.w = 0.0f;

    attach_audio_source(filter, obs_data_get_string(settings, "audio_source"));
    filter->audio_threshold_db = (float)obs_data_get_double(settings, "audio_threshold");
    filter->talk_hold_sec = (float)obs_data_get_double(settings, "talk_hold_ms") / 1000.0f;

    attach_spec_source(filter, obs_data_get_string(settings, "spec_source"));
    filter->spec_layer = (int)obs_data_get_int(settings, "spec_layer");
    filter->spec_design = (int)obs_data_get_int(settings, "spec_design");
    vec4_from_rgba(&filter->spec_color, (uint32_t)obs_data_get_int(settings, "spec_color"));
    filter->spec_sensitivity = (float)obs_data_get_double(settings, "spec_sensitivity");
    filter->spec_height = (float)obs_data_get_double(settings, "spec_height");
    filter->spec_opacity = (float)obs_data_get_double(settings, "spec_opacity");

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
    filter->scream_threshold_db = (float)obs_data_get_double(settings, "scream_threshold_db");
    filter->scream_shake_amp = (float)obs_data_get_double(settings, "scream_shake_amp");
    filter->idle_dim_amp = (float)obs_data_get_double(settings, "idle_dim_amp");

    filter->bg_mode = (int)obs_data_get_int(settings, "bg_mode");
    vec4_from_rgba(&filter->bg_color, (uint32_t)obs_data_get_int(settings, "bg_color"));
    filter->bg_blur = (float)obs_data_get_double(settings, "bg_blur") / 1000.0f;
}

static void *ubique_create(obs_data_t *settings, obs_source_t *context)
{
    struct ubique_cam_data *filter = bzalloc(sizeof(struct ubique_cam_data));
    filter->context = context;
    filter->current_db = -100.0f;
    filter->blink_timer = 3.0f;
    filter->hotkey_toggle = OBS_INVALID_HOTKEY_ID;
    init_goertzel_coeffs(filter);

    char *effect_path = obs_module_file("effects/ubique_cam.effect");
    obs_enter_graphics();
    uint32_t zero_px = 0;
    const uint8_t *px_ptr = (const uint8_t *)&zero_px;
    filter->dummy_tex = gs_texture_create(1, 1, GS_RGBA, 1, &px_ptr, 0);
    filter->spec_tex = gs_texture_create(SPEC_BANDS, 1, GS_RGBA, 1, NULL, GS_DYNAMIC);

    filter->effect = gs_effect_create_from_file(effect_path, NULL);
    if (filter->effect) {
        filter->param_avatar_tex        = gs_effect_get_param_by_name(filter->effect, "avatar_tex");
        filter->param_bg_tex            = gs_effect_get_param_by_name(filter->effect, "bg_tex");
        filter->param_spec_tex          = gs_effect_get_param_by_name(filter->effect, "spec_tex");
        filter->param_out_size          = gs_effect_get_param_by_name(filter->effect, "out_size");
        filter->param_src_size          = gs_effect_get_param_by_name(filter->effect, "src_size");
        filter->param_pad_offset        = gs_effect_get_param_by_name(filter->effect, "pad_offset");
        filter->param_time              = gs_effect_get_param_by_name(filter->effect, "time");

        filter->param_morph_t           = gs_effect_get_param_by_name(filter->effect, "morph_t");
        filter->param_trans_type        = gs_effect_get_param_by_name(filter->effect, "trans_type");

        filter->param_crop              = gs_effect_get_param_by_name(filter->effect, "crop");
        filter->param_zoom              = gs_effect_get_param_by_name(filter->effect, "zoom");
        filter->param_pan               = gs_effect_get_param_by_name(filter->effect, "pan");
        filter->param_flip_h            = gs_effect_get_param_by_name(filter->effect, "flip_h");
        filter->param_vignette          = gs_effect_get_param_by_name(filter->effect, "vignette");
        filter->param_contrast          = gs_effect_get_param_by_name(filter->effect, "contrast");
        filter->param_warmth            = gs_effect_get_param_by_name(filter->effect, "warmth");

        filter->param_corner_radius     = gs_effect_get_param_by_name(filter->effect, "corner_radius");
        filter->param_border_width      = gs_effect_get_param_by_name(filter->effect, "border_width");
        filter->param_border_style      = gs_effect_get_param_by_name(filter->effect, "border_style");
        filter->param_border_grad_speed = gs_effect_get_param_by_name(filter->effect, "border_grad_speed");
        filter->param_border_color      = gs_effect_get_param_by_name(filter->effect, "border_color");
        filter->param_border_color2     = gs_effect_get_param_by_name(filter->effect, "border_color2");

        filter->param_glow_size         = gs_effect_get_param_by_name(filter->effect, "glow_size");
        filter->param_glow_intensity    = gs_effect_get_param_by_name(filter->effect, "glow_intensity");
        filter->param_glow_color        = gs_effect_get_param_by_name(filter->effect, "glow_color");
        filter->param_audio_level       = gs_effect_get_param_by_name(filter->effect, "audio_level");
        filter->param_peak_level        = gs_effect_get_param_by_name(filter->effect, "peak_level");
        filter->param_peak_color        = gs_effect_get_param_by_name(filter->effect, "peak_color");

        filter->param_shadow_blur       = gs_effect_get_param_by_name(filter->effect, "shadow_blur");
        filter->param_shadow_offset     = gs_effect_get_param_by_name(filter->effect, "shadow_offset");
        filter->param_shadow_color      = gs_effect_get_param_by_name(filter->effect, "shadow_color");

        filter->param_has_avatar        = gs_effect_get_param_by_name(filter->effect, "has_avatar");
        filter->param_avatar_tex_size   = gs_effect_get_param_by_name(filter->effect, "avatar_tex_size");
        filter->param_avatar_scale      = gs_effect_get_param_by_name(filter->effect, "avatar_scale");
        filter->param_avatar_offset     = gs_effect_get_param_by_name(filter->effect, "avatar_offset");
        filter->param_anim_breath       = gs_effect_get_param_by_name(filter->effect, "anim_breath");
        filter->param_anim_bounce       = gs_effect_get_param_by_name(filter->effect, "anim_bounce");
        filter->param_anim_sway         = gs_effect_get_param_by_name(filter->effect, "anim_sway");
        filter->param_shake_offset      = gs_effect_get_param_by_name(filter->effect, "shake_offset");
        filter->param_idle_dim_factor   = gs_effect_get_param_by_name(filter->effect, "idle_dim_factor");

        filter->param_bg_mode           = gs_effect_get_param_by_name(filter->effect, "bg_mode");
        filter->param_bg_color          = gs_effect_get_param_by_name(filter->effect, "bg_color");
        filter->param_bg_blur           = gs_effect_get_param_by_name(filter->effect, "bg_blur");

        filter->param_spec_layer        = gs_effect_get_param_by_name(filter->effect, "spec_layer");
        filter->param_spec_design       = gs_effect_get_param_by_name(filter->effect, "spec_design");
        filter->param_spec_color        = gs_effect_get_param_by_name(filter->effect, "spec_color");
        filter->param_spec_height       = gs_effect_get_param_by_name(filter->effect, "spec_height");
        filter->param_spec_opacity      = gs_effect_get_param_by_name(filter->effect, "spec_opacity");
    } else {
        blog(LOG_ERROR, "[ubique-cam] Failed to load effect: %s", effect_path ? effect_path : "NULL");
    }
    obs_leave_graphics();
    bfree(effect_path);

    filter->hotkey_toggle = obs_hotkey_register_source(
        context, "ubique_cam_toggle_mode", obs_module_text("HotkeyToggle"),
        hotkey_toggle_cb, filter);

    ubique_update(filter, settings);
    filter->morph_t = (filter->mode == MODE_PNGTUBER) ? 1.0f : 0.0f;
    ubique_ui_register_filter(filter);
    return filter;
}

static void ubique_destroy(void *data)
{
    struct ubique_cam_data *filter = data;
    ubique_ui_unregister_filter(filter);

    if (filter->hotkey_toggle != OBS_INVALID_HOTKEY_ID) {
        obs_hotkey_unregister(filter->hotkey_toggle);
    }
    detach_audio_source(filter);
    detach_spec_source(filter);
    bfree(filter->audio_source_name);
    bfree(filter->spec_source_name);

    free_tex_slot(&filter->img_idle);
    free_tex_slot(&filter->img_talk);
    free_tex_slot(&filter->img_blink);
    free_tex_slot(&filter->img_talk_blink);
    free_tex_slot(&filter->img_bg);

    obs_enter_graphics();
    if (filter->spec_tex) gs_texture_destroy(filter->spec_tex);
    if (filter->dummy_tex) gs_texture_destroy(filter->dummy_tex);
    if (filter->effect) gs_effect_destroy(filter->effect);
    obs_leave_graphics();

    bfree(filter);
}

static void ubique_video_tick(void *data, float seconds)
{
    struct ubique_cam_data *filter = data;
    filter->time_acc += seconds;

    // 1. Mode transition
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

    // 2. Talking state & idle dim
    float db = filter->current_db;
    if (db >= filter->audio_threshold_db) {
        filter->is_talking = true;
        filter->talk_hold_timer = filter->talk_hold_sec;
        filter->silent_timer = 0.0f;
    } else if (filter->talk_hold_timer > 0.0f) {
        filter->talk_hold_timer -= seconds;
        if (filter->talk_hold_timer <= 0.0f) {
            filter->is_talking = false;
        }
    } else {
        filter->is_talking = false;
        filter->silent_timer += seconds;
    }

    float target_dim = (filter->silent_timer > 1.5f) ? filter->idle_dim_amp : 0.0f;
    float dim_rate = fminf(seconds * 4.0f, 1.0f);
    filter->cur_idle_dim += (target_dim - filter->cur_idle_dim) * dim_rate;

    // 3. Audio-reactive level & peak level
    float norm_audio = 0.0f;
    if (filter->border_audio_react && db > filter->audio_threshold_db - 6.0f) {
        norm_audio = (db - (filter->audio_threshold_db - 6.0f)) / fmaxf(1.0f, -6.0f - (filter->audio_threshold_db - 6.0f));
        if (norm_audio < 0.0f) norm_audio = 0.0f;
        if (norm_audio > 1.0f) norm_audio = 1.0f;
    }
    float target_audio = norm_audio * filter->border_react_strength;
    float atk_dec = (target_audio > filter->smooth_audio_level) ? fminf(seconds * 22.0f, 1.0f) : fminf(seconds * 7.0f, 1.0f);
    filter->smooth_audio_level += (target_audio - filter->smooth_audio_level) * atk_dec;

    float target_peak = (filter->border_audio_react && db >= filter->peak_threshold_db) ? 1.0f : 0.0f;
    float peak_rate = (target_peak > filter->smooth_peak_level) ? fminf(seconds * 25.0f, 1.0f) : fminf(seconds * 5.0f, 1.0f);
    filter->smooth_peak_level += (target_peak - filter->smooth_peak_level) * peak_rate;

    // 4. Bounce on speech onset
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

    // 5. Auto-blink timer
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

    // 6. Smooth 32-band audio spectrum
    if (filter->spec_layer != SPEC_LAYER_OFF) {
        float atk = fminf(seconds * 28.0f, 1.0f);
        float dec = fminf(seconds * 9.0f, 1.0f);
        for (int b = 0; b < SPEC_BANDS; b++) {
            float target = filter->raw_bands[b] * filter->spec_sensitivity;
            if (target > 1.0f) target = 1.0f;
            float k = (target > filter->smooth_bands[b]) ? atk : dec;
            filter->smooth_bands[b] += (target - filter->smooth_bands[b]) * k;
            uint8_t v = (uint8_t)(fminf(fmaxf(filter->smooth_bands[b], 0.0f), 1.0f) * 255.0f);
            filter->spec_rgba[b * 4 + 0] = v;
            filter->spec_rgba[b * 4 + 1] = v;
            filter->spec_rgba[b * 4 + 2] = v;
            filter->spec_rgba[b * 4 + 3] = 255;
        }
    }
}

static void ubique_calc_dimensions(struct ubique_cam_data *filter, uint32_t *base_w, uint32_t *base_h)
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

    float react_extra = filter->border_audio_react ? (filter->glow_size * 0.6f * filter->border_react_strength) : 0.0f;
    float spec_extra = (filter->spec_layer == SPEC_LAYER_BORDER) ? (filter->spec_height * 120.0f) : 0.0f;
    float extra = filter->border_width + filter->glow_size + react_extra + spec_extra;

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

static uint32_t ubique_get_width(void *data)
{
    struct ubique_cam_data *filter = data;
    uint32_t bw, bh;
    ubique_calc_dimensions(filter, &bw, &bh);
    return filter->out_width;
}

static uint32_t ubique_get_height(void *data)
{
    struct ubique_cam_data *filter = data;
    uint32_t bw, bh;
    ubique_calc_dimensions(filter, &bw, &bh);
    return filter->out_height;
}

static struct tex_slot *pick_active_avatar(struct ubique_cam_data *filter)
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

static void ubique_video_render(void *data, gs_effect_t *effect)
{
    UNUSED_PARAMETER(effect);
    struct ubique_cam_data *filter = data;

    if (!filter->effect) {
        obs_source_skip_video_filter(filter->context);
        return;
    }

    uint32_t base_w = 0, base_h = 0;
    ubique_calc_dimensions(filter, &base_w, &base_h);
    if (base_w == 0 || base_h == 0) {
        obs_source_skip_video_filter(filter->context);
        return;
    }

    if (filter->spec_tex && filter->spec_layer != SPEC_LAYER_OFF) {
        gs_texture_set_image(filter->spec_tex, filter->spec_rgba, SPEC_BANDS * 4, false);
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
    gs_texture_t *sp_tex = filter->spec_tex ? filter->spec_tex : filter->dummy_tex;

    float breath = sinf(filter->time_acc * 2.2f) * filter->anim_breath_amp;
    float sway = sinf(filter->time_acc * 1.4f) * filter->anim_sway_amp * 0.0174533f;
    float bounce = filter->bounce_active
                       ? (sinf(filter->bounce_phase * 3.1415926f) * filter->anim_bounce_amp)
                       : 0.0f;

    struct vec2 shake_off = {0.0f, 0.0f};
    if (filter->scream_shake_amp > 0.001f && filter->current_db >= filter->scream_threshold_db) {
        shake_off.x = sinf(filter->time_acc * 73.0f) * filter->scream_shake_amp;
        shake_off.y = cosf(filter->time_acc * 91.0f) * filter->scream_shake_amp;
    }

    struct vec2 out_size = {(float)filter->out_width, (float)filter->out_height};
    struct vec2 src_size = {(float)base_w, (float)base_h};
    struct vec2 pad_offset = {filter->pad_left, filter->pad_top};
    struct vec4 crop = {filter->crop_left, filter->crop_top, filter->crop_right, filter->crop_bottom};
    struct vec2 pan = {filter->pan_x, filter->pan_y};
    struct vec2 shadow_offset = {filter->shadow_offset_x, filter->shadow_offset_y};
    struct vec2 av_offset = {filter->avatar_offset_x, filter->avatar_offset_y};

    gs_effect_set_texture(filter->param_avatar_tex, av_tex);
    gs_effect_set_texture(filter->param_bg_tex, bg_tex);
    gs_effect_set_texture(filter->param_spec_tex, sp_tex);
    gs_effect_set_vec2(filter->param_out_size, &out_size);
    gs_effect_set_vec2(filter->param_src_size, &src_size);
    gs_effect_set_vec2(filter->param_pad_offset, &pad_offset);
    gs_effect_set_float(filter->param_time, filter->time_acc);

    gs_effect_set_float(filter->param_morph_t, filter->morph_t);
    gs_effect_set_int(filter->param_trans_type, filter->transition_type);

    gs_effect_set_vec4(filter->param_crop, &crop);
    gs_effect_set_float(filter->param_zoom, filter->zoom);
    gs_effect_set_vec2(filter->param_pan, &pan);
    gs_effect_set_int(filter->param_flip_h, filter->flip_h ? 1 : 0);
    gs_effect_set_float(filter->param_vignette, filter->vignette);
    gs_effect_set_float(filter->param_contrast, filter->contrast);
    gs_effect_set_float(filter->param_warmth, filter->warmth);

    gs_effect_set_float(filter->param_corner_radius, filter->corner_radius);
    gs_effect_set_float(filter->param_border_width, filter->border_width);
    gs_effect_set_int(filter->param_border_style, filter->border_style);
    gs_effect_set_float(filter->param_border_grad_speed, filter->border_grad_speed);
    gs_effect_set_vec4(filter->param_border_color, &filter->border_color);
    gs_effect_set_vec4(filter->param_border_color2, &filter->border_color2);

    gs_effect_set_float(filter->param_glow_size, filter->glow_size);
    gs_effect_set_float(filter->param_glow_intensity, filter->glow_intensity);
    gs_effect_set_vec4(filter->param_glow_color, &filter->glow_color);
    gs_effect_set_float(filter->param_audio_level, filter->smooth_audio_level);
    gs_effect_set_float(filter->param_peak_level, filter->smooth_peak_level);
    gs_effect_set_vec4(filter->param_peak_color, &filter->peak_color);

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
    gs_effect_set_vec2(filter->param_shake_offset, &shake_off);
    gs_effect_set_float(filter->param_idle_dim_factor, filter->cur_idle_dim);

    gs_effect_set_int(filter->param_bg_mode, filter->bg_mode);
    gs_effect_set_vec4(filter->param_bg_color, &filter->bg_color);
    gs_effect_set_float(filter->param_bg_blur, filter->bg_blur);

    gs_effect_set_int(filter->param_spec_layer, filter->spec_layer);
    gs_effect_set_int(filter->param_spec_design, filter->spec_design);
    gs_effect_set_vec4(filter->param_spec_color, &filter->spec_color);
    gs_effect_set_float(filter->param_spec_height, filter->spec_height);
    gs_effect_set_float(filter->param_spec_opacity, filter->spec_opacity);

    gs_blend_state_push();
    gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

    obs_source_process_filter_end(filter->context, filter->effect, filter->out_width, filter->out_height);

    gs_blend_state_pop();
}

static void ubique_get_defaults(obs_data_t *settings)
{
    obs_data_set_default_int(settings, "mode", MODE_WEBCAM);
    obs_data_set_default_int(settings, "transition_type", TRANS_BLUR);
    obs_data_set_default_double(settings, "transition_speed", 0.35);

    obs_data_set_default_double(settings, "corner_radius", 32.0);
    obs_data_set_default_double(settings, "zoom", 1.0);
    obs_data_set_default_double(settings, "pan_x", 0.0);
    obs_data_set_default_double(settings, "pan_y", 0.0);
    obs_data_set_default_double(settings, "crop_left", 0.0);
    obs_data_set_default_double(settings, "crop_right", 0.0);
    obs_data_set_default_double(settings, "crop_top", 0.0);
    obs_data_set_default_double(settings, "crop_bottom", 0.0);
    obs_data_set_default_bool(settings, "flip_h", false);
    obs_data_set_default_double(settings, "vignette", 0.0);
    obs_data_set_default_double(settings, "contrast", 1.0);
    obs_data_set_default_double(settings, "warmth", 0.0);

    obs_data_set_default_double(settings, "border_width", 3.0);
    obs_data_set_default_int(settings, "border_style", BORDER_SOLID);
    obs_data_set_default_double(settings, "border_grad_speed", 1.0);
    obs_data_set_default_int(settings, "border_color", 0xFFE2E9EC);
    obs_data_set_default_int(settings, "border_color2", 0xFF649EFF);

    obs_data_set_default_double(settings, "glow_size", 18.0);
    obs_data_set_default_double(settings, "glow_intensity", 0.65);
    obs_data_set_default_int(settings, "glow_color", 0xFF649EFF);
    obs_data_set_default_bool(settings, "border_audio_react", false);
    obs_data_set_default_double(settings, "border_react_strength", 1.0);
    obs_data_set_default_double(settings, "peak_threshold_db", -12.0);
    obs_data_set_default_int(settings, "peak_color", 0xFF475AE0);

    obs_data_set_default_bool(settings, "shadow_enable", true);
    obs_data_set_default_double(settings, "shadow_blur", 24.0);
    obs_data_set_default_double(settings, "shadow_offset_x", 0.0);
    obs_data_set_default_double(settings, "shadow_offset_y", 12.0);
    obs_data_set_default_int(settings, "shadow_color", 0xB4000000);

    obs_data_set_default_double(settings, "audio_threshold", -38.0);
    obs_data_set_default_double(settings, "talk_hold_ms", 150.0);

    obs_data_set_default_int(settings, "spec_layer", SPEC_LAYER_OFF);
    obs_data_set_default_int(settings, "spec_design", SPEC_DESIGN_BARS);
    obs_data_set_default_int(settings, "spec_color", 0xFF649EFF);
    obs_data_set_default_double(settings, "spec_sensitivity", 1.2);
    obs_data_set_default_double(settings, "spec_height", 0.35);
    obs_data_set_default_double(settings, "spec_opacity", 0.85);

    obs_data_set_default_double(settings, "blink_interval", 3.5);
    obs_data_set_default_double(settings, "blink_duration", 120.0);
    obs_data_set_default_double(settings, "avatar_scale", 0.9);
    obs_data_set_default_double(settings, "avatar_offset_x", 0.0);
    obs_data_set_default_double(settings, "avatar_offset_y", 0.0);

    obs_data_set_default_double(settings, "anim_breath", 0.02);
    obs_data_set_default_double(settings, "anim_bounce", 0.06);
    obs_data_set_default_double(settings, "anim_sway", 1.5);
    obs_data_set_default_double(settings, "scream_threshold_db", -8.0);
    obs_data_set_default_double(settings, "scream_shake_amp", 0.02);
    obs_data_set_default_double(settings, "idle_dim_amp", 0.0);

    obs_data_set_default_int(settings, "bg_mode", BG_BLUR_CAM);
    obs_data_set_default_int(settings, "bg_color", 0xFF141211);
    obs_data_set_default_double(settings, "bg_blur", 18.0);
}

static bool open_settings_cb(obs_properties_t *props, obs_property_t *property, void *data)
{
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(property);
    struct ubique_cam_data *filter = data;
    ubique_ui_open(filter);
    return false;
}

// CLEAN & MINIMAL NATIVE PROPERTIES: ONLY Mode + "Настройка UBIQUE CAM"
static obs_properties_t *ubique_get_properties(void *data)
{
    obs_properties_t *props = obs_properties_create();

    obs_property_t *mode_list = obs_properties_add_list(
        props, "mode", obs_module_text("Mode"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(mode_list, obs_module_text("ModeWebcam"), MODE_WEBCAM);
    obs_property_list_add_int(mode_list, obs_module_text("ModePNGTuber"), MODE_PNGTUBER);

    obs_properties_add_button2(props, "open_ubique_ui", obs_module_text("OpenSettingsBtn"),
                               open_settings_cb, data);

    return props;
}

struct obs_source_info ubique_cam_filter_info = {
    .id             = "ubique_cam_filter",
    .type           = OBS_SOURCE_TYPE_FILTER,
    .output_flags   = OBS_SOURCE_VIDEO,
    .get_name       = ubique_get_name,
    .create         = ubique_create,
    .destroy        = ubique_destroy,
    .update         = ubique_update,
    .get_defaults   = ubique_get_defaults,
    .get_properties = ubique_get_properties,
    .video_tick     = ubique_video_tick,
    .video_render   = ubique_video_render,
    .get_width      = ubique_get_width,
    .get_height     = ubique_get_height,
};

bool obs_module_load(void)
{
    obs_register_source(&ubique_cam_filter_info);
    blog(LOG_INFO, "[ubique-cam] UBIQUE CAM loaded successfully (v0.5.0 Max Edition)");
    return true;
}

void obs_module_post_load(void)
{
    ubique_ui_init_dock();
}

void obs_module_unload(void)
{
    ubique_ui_destroy_dock();
    blog(LOG_INFO, "[ubique-cam] UBIQUE CAM unloaded");
}
