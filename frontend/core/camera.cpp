//
// Created by 66 on 2025/12/18.
//

#include "camera.h"

Camera::Camera(std::string device_description)
    : m_device_description(std::move(device_description)) {
    av_log_set_level(AV_LOG_INFO);

    // 构建设备 URL
    if (m_device_description.empty()) {
#ifdef _WIN32
        m_device_url = "video=USB Video Device";
#else
        m_device_url = "0";
#endif
    } else {
#ifdef _WIN32
        m_device_url = "video=" + m_device_description;
#else
        m_device_url = m_device_description;
#endif
    }

}

const char* Camera::get_input_format_name() const {
#ifdef _WIN32
    return "dshow";
#else
    return "avfoundation";
#endif
}

const char* Camera::get_device_name() const {
    return m_device_url.c_str();
}

void Camera::setup_options(AVDictionary** opts) {
    av_dict_set(opts, "video_size", "1280x720", 0);
    av_dict_set(opts, "framerate", "30", 0);
    av_dict_set(opts, "rtbufsize", "2048k", 0);
}