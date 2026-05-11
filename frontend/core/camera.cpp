//
// Created by 66 on 2025/12/18.
//

#include "camera.h"

Camera::Camera() {
    av_log_set_level(AV_LOG_INFO);
    init_ctx();
}

const char* Camera::get_input_format_name() const {
#ifdef _WIN32
    return "dshow";
#else
    return "avfoundation";
#endif
}

const char* Camera::get_device_name() const {
#ifdef _WIN32
    return "video=USB Video Device";
#else
    return "0";
#endif
}

void Camera::setup_options(AVDictionary** opts) {
    av_dict_set(opts, "video_size", "1280x720", 0);
    av_dict_set(opts, "framerate", "30", 0);
    av_dict_set(opts, "rtbufsize", "2048k", 0);
}