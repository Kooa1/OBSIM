#include "screencaptor.h"

ScreenCaptor::ScreenCaptor() {}

void ScreenCaptor::apply_config(const CaptorConfig &config) {
    VideoCaptor::apply_config(config);
}

void ScreenCaptor::setup_options(AVDictionary** opts) {
    av_dict_set(opts, "framerate", "60", 0);
    if (m_config.width > 0 && m_config.height > 0) {
        std::string video_size = std::to_string(m_config.width) + "x" + std::to_string(m_config.height);
        av_dict_set(opts, "video_size", video_size.c_str(), 0);
    }
    if (m_config.offset_x != 0 || m_config.offset_y != 0) {
        av_dict_set(opts, "offset_x", std::to_string(m_config.offset_x).c_str(), 0);
        av_dict_set(opts, "offset_y", std::to_string(m_config.offset_y).c_str(), 0);
    }
}
