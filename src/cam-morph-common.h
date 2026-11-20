#pragma once
#include <obs-module.h>

#ifdef __cplusplus
extern "C" {
#endif

enum morph_mode {
    MODE_WEBCAM = 0,
    MODE_PNGTUBER = 1
};

enum png_bg_mode {
    BG_COLOR = 0,
    BG_IMAGE = 1,
    BG_BLUR_CAM = 2,
    BG_TRANSPARENT = 3
};

struct cam_morph_data;

// Accessors for UI
obs_source_t *cam_morph_get_source(struct cam_morph_data *filter);
float cam_morph_get_current_db(struct cam_morph_data *filter);
bool cam_morph_is_talking(struct cam_morph_data *filter);

// UI bridge (implemented in cam-morph-ui.cpp)
void cam_morph_ui_open(struct cam_morph_data *filter);
void cam_morph_ui_close_for_filter(struct cam_morph_data *filter);

#ifdef __cplusplus
}
#endif
