#include "micaudiocaptor.h"

MicAudioCaptor::MicAudioCaptor() {
    m_device_name = "audio=麦克风 (Lian ll)";

    init_ctx();
}

const char* MicAudioCaptor::get_input_format_name() const {
    return "dshow";
}

const char* MicAudioCaptor::get_device_name() const {
    return m_device_name.c_str();
}

void MicAudioCaptor::setup_options(AVDictionary** opts) {
    av_dict_set(opts, "sample_rate", "44100", 0);
    av_dict_set(opts, "channels", "2", 0);
}