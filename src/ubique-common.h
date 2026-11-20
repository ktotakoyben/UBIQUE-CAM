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

struct ubique_cam_data;

obs_source_t *ubique_get_source(struct ubique_cam_data *filter);
float ubique_get_current_db(struct ubique_cam_data *filter);
bool ubique_is_talking(struct ubique_cam_data *filter);

void ubique_ui_open(struct ubique_cam_data *filter);
void ubique_ui_close_for_filter(struct ubique_cam_data *filter);

#ifdef __cplusplus
}
#endif
