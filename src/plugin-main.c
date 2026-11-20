#include <obs-module.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <math.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-cam-morph", "en-US")

struct cam_morph_data {
    obs_source_t *context;
    gs_effect_t *effect;

    // Effect parameters
    gs_eparam_t *param_out_size;
    gs_eparam_t *param_src_size;
    gs_eparam_t *param_pad_offset;
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

    // Settings
    float corner_radius;
    float zoom;
    float pan_x;
    float pan_y;

    float crop_left;
    float crop_right;
    float crop_top;
    float crop_bottom;

    float border_width;
    struct vec4 border_color;

    float glow_size;
    float glow_intensity;
    struct vec4 glow_color;

    bool shadow_enable;
    float shadow_blur;
    float shadow_offset_x;
    float shadow_offset_y;
    struct vec4 shadow_color;

    // Calculated dimensions
    uint32_t out_width;
    uint32_t out_height;
    float pad_left;
    float pad_top;
};

static const char *cam_morph_get_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return obs_module_text("CamMorphFilter");
}

static void cam_morph_update(void *data, obs_data_t *settings)
{
    struct cam_morph_data *filter = data;

    filter->corner_radius = (float)obs_data_get_double(settings, "corner_radius");
    filter->zoom = (float)obs_data_get_double(settings, "zoom");
    filter->pan_x = (float)obs_data_get_double(settings, "pan_x");
    filter->pan_y = (float)obs_data_get_double(settings, "pan_y");

    filter->crop_left = (float)obs_data_get_double(settings, "crop_left") / 100.0f;
    filter->crop_right = (float)obs_data_get_double(settings, "crop_right") / 100.0f;
    filter->crop_top = (float)obs_data_get_double(settings, "crop_top") / 100.0f;
    filter->crop_bottom = (float)obs_data_get_double(settings, "crop_bottom") / 100.0f;

    // Prevent inverted crop
    if (filter->crop_left + filter->crop_right > 0.95f) {
        filter->crop_right = 0.95f - filter->crop_left;
    }
    if (filter->crop_top + filter->crop_bottom > 0.95f) {
        filter->crop_bottom = 0.95f - filter->crop_top;
    }

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
    if (!filter->shadow_enable) {
        filter->shadow_color.w = 0.0f;
    }
}

static void *cam_morph_create(obs_data_t *settings, obs_source_t *context)
{
    struct cam_morph_data *filter = bzalloc(sizeof(struct cam_morph_data));
    filter->context = context;

    char *effect_path = obs_module_file("effects/cam_morph.effect");
    obs_enter_graphics();
    filter->effect = gs_effect_create_from_file(effect_path, NULL);
    if (filter->effect) {
        filter->param_out_size      = gs_effect_get_param_by_name(filter->effect, "out_size");
        filter->param_src_size      = gs_effect_get_param_by_name(filter->effect, "src_size");
        filter->param_pad_offset    = gs_effect_get_param_by_name(filter->effect, "pad_offset");
        filter->param_crop          = gs_effect_get_param_by_name(filter->effect, "crop");
        filter->param_zoom          = gs_effect_get_param_by_name(filter->effect, "zoom");
        filter->param_pan           = gs_effect_get_param_by_name(filter->effect, "pan");
        filter->param_corner_radius = gs_effect_get_param_by_name(filter->effect, "corner_radius");
        filter->param_border_width  = gs_effect_get_param_by_name(filter->effect, "border_width");
        filter->param_border_color  = gs_effect_get_param_by_name(filter->effect, "border_color");
        filter->param_glow_size     = gs_effect_get_param_by_name(filter->effect, "glow_size");
        filter->param_glow_intensity= gs_effect_get_param_by_name(filter->effect, "glow_intensity");
        filter->param_glow_color    = gs_effect_get_param_by_name(filter->effect, "glow_color");
        filter->param_shadow_blur   = gs_effect_get_param_by_name(filter->effect, "shadow_blur");
        filter->param_shadow_offset = gs_effect_get_param_by_name(filter->effect, "shadow_offset");
        filter->param_shadow_color  = gs_effect_get_param_by_name(filter->effect, "shadow_color");
    } else {
        blog(LOG_ERROR, "[obs-cam-morph] Failed to load effect: %s", effect_path ? effect_path : "NULL");
    }
    obs_leave_graphics();
    bfree(effect_path);

    cam_morph_update(filter, settings);
    return filter;
}

static void cam_morph_destroy(void *data)
{
    struct cam_morph_data *filter = data;
    if (filter->effect) {
        obs_enter_graphics();
        gs_effect_destroy(filter->effect);
        obs_leave_graphics();
    }
    bfree(filter);
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

    struct vec2 out_size = {(float)filter->out_width, (float)filter->out_height};
    struct vec2 src_size = {(float)base_w, (float)base_h};
    struct vec2 pad_offset = {filter->pad_left, filter->pad_top};
    struct vec4 crop = {filter->crop_left, filter->crop_top, filter->crop_right, filter->crop_bottom};
    struct vec2 pan = {filter->pan_x, filter->pan_y};
    struct vec2 shadow_offset = {filter->shadow_offset_x, filter->shadow_offset_y};

    gs_effect_set_vec2(filter->param_out_size, &out_size);
    gs_effect_set_vec2(filter->param_src_size, &src_size);
    gs_effect_set_vec2(filter->param_pad_offset, &pad_offset);
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

    gs_blend_state_push();
    gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

    obs_source_process_filter_end(filter->context, filter->effect, filter->out_width, filter->out_height);

    gs_blend_state_pop();
}

static void cam_morph_get_defaults(obs_data_t *settings)
{
    obs_data_set_default_double(settings, "corner_radius", 32.0);
    obs_data_set_default_double(settings, "zoom", 1.0);
    obs_data_set_default_double(settings, "pan_x", 0.0);
    obs_data_set_default_double(settings, "pan_y", 0.0);

    obs_data_set_default_double(settings, "crop_left", 0.0);
    obs_data_set_default_double(settings, "crop_right", 0.0);
    obs_data_set_default_double(settings, "crop_top", 0.0);
    obs_data_set_default_double(settings, "crop_bottom", 0.0);

    obs_data_set_default_double(settings, "border_width", 4.0);
    obs_data_set_default_int(settings, "border_color", 0xFFFFFFFF); // White

    obs_data_set_default_double(settings, "glow_size", 20.0);
    obs_data_set_default_double(settings, "glow_intensity", 0.8);
    obs_data_set_default_int(settings, "glow_color", 0xFFFF8800); // Cyan/Blueish in ABGR

    obs_data_set_default_bool(settings, "shadow_enable", true);
    obs_data_set_default_double(settings, "shadow_blur", 24.0);
    obs_data_set_default_double(settings, "shadow_offset_x", 0.0);
    obs_data_set_default_double(settings, "shadow_offset_y", 12.0);
    obs_data_set_default_int(settings, "shadow_color", 0xB4000000); // Semi-transparent black
}

static obs_properties_t *cam_morph_get_properties(void *data)
{
    UNUSED_PARAMETER(data);
    obs_properties_t *props = obs_properties_create();

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
    .video_render   = cam_morph_video_render,
    .get_width      = cam_morph_get_width,
    .get_height     = cam_morph_get_height,
};

bool obs_module_load(void)
{
    obs_register_source(&cam_morph_filter_info);
    blog(LOG_INFO, "[obs-cam-morph] Plugin loaded successfully (v0.1.0)");
    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "[obs-cam-morph] Plugin unloaded");
}
