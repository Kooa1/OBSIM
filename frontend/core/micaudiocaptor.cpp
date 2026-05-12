#include "micaudiocaptor.h"

MicAudioCaptor::MicAudioCaptor() {
    init_ctx();
    qDebug() << "MicAudioCaptor initialized:" << m_initialized;
}

const char* MicAudioCaptor::get_input_format_name() const {
    return "dshow";
}

const char* MicAudioCaptor::get_device_name() const {
    return "audio=麦克风 (USB Audio Device)";
}

void MicAudioCaptor::setup_options(AVDictionary** opts) {
    av_dict_set(opts, "sample_rate", "44100", 0);
    av_dict_set(opts, "channels", "2", 0);
}