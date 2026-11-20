#pragma once
#include <obs-module.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ubique_mode {
    MODE_WEBCAM = 0,
    MODE_PNGTUBER = 1
};

enum png_bg_mode {
    BG_COLOR = 0,
    BG_IMAGE = 1,
    BG_BLUR_CAM = 2,
    BG_TRANSPARENT = 3
};

enum border_style_mode {
    BORDER_SOLID = 0,
    BORDER_GRADIENT = 1,
    BORDER_ANIM_GRADIENT = 2
};

enum transition_style_mode {
    TRANS_CROSSFADE = 0,
    TRANS_BLUR = 1,
    TRANS_GLITCH = 2,
    TRANS_CRT = 3
};

enum spec_layer_mode {
    SPEC_LAYER_OFF = 0,
    SPEC_LAYER_BEHIND_AVATAR = 1,
    SPEC_LAYER_OVERLAY = 2,
    SPEC_LAYER_BORDER = 3
};

enum spec_design_mode {
    SPEC_DESIGN_BARS = 0,
    SPEC_DESIGN_WAVE = 1,
    SPEC_DESIGN_MIRROR = 2,
    SPEC_DESIGN_LED = 3
};

struct ubique_cam_data;

obs_source_t *ubique_get_source(struct ubique_cam_data *filter);
float ubique_get_current_db(struct ubique_cam_data *filter);
bool ubique_is_talking(struct ubique_cam_data *filter);

void ubique_ui_register_filter(struct ubique_cam_data *filter);
void ubique_ui_unregister_filter(struct ubique_cam_data *filter);
void ubique_ui_open(struct ubique_cam_data *filter);
void ubique_ui_init_dock(void);
void ubique_ui_destroy_dock(void);

#ifdef __cplusplus
}
#endif
