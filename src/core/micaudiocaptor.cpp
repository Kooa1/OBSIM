#include "micaudiocaptor.h"

extern "C" {
#include <libavdevice/avdevice.h>
}

MicAudioCaptor::MicAudioCaptor() {
    const AVInputFormat *fmt = av_find_input_format("dshow");
    if (fmt) {
        AVDeviceInfoList *device_list = nullptr;
        if (avdevice_list_input_sources(fmt, nullptr, nullptr, &device_list) >= 0 && device_list) {
            int def_idx = device_list->default_device;
            if (def_idx >= 0 && def_idx < device_list->nb_devices && device_list->devices[def_idx]) {
                m_device_name = "audio=" + std::string(device_list->devices[def_idx]->device_name);
            }
            avdevice_free_list_devices(&device_list);
        }
    }
    init_ctx();
}

MicAudioCaptor::MicAudioCaptor(const std::string &device_name)
    : m_device_name("audio=" + device_name) {
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