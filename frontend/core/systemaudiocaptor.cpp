#include "systemaudiocaptor.h"

SystemAudioCaptor::SystemAudioCaptor() {
    av_log_set_level(AV_LOG_DEBUG); // 临时启用调试日志
    init_ctx();
    qDebug() << "SystemAudioCaptor initialized:" << m_initialized;
}

const char *SystemAudioCaptor::get_input_format_name() const {
    return "dshow";
}

const char *SystemAudioCaptor::get_device_name() const {
    return "audio=立体声混音 (Realtek High Definition Audio)";
}

void SystemAudioCaptor::setup_options(AVDictionary **opts) {
    av_dict_set(opts, "sample_rate", "44100", 0);
    av_dict_set(opts, "channels", "2", 0);
}
