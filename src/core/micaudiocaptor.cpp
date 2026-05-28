#include "micaudiocaptor.h"
#include <QAudioDevice>
#include <QMediaDevices>

extern "C" {
#include <libavdevice/avdevice.h>
}

MicAudioCaptor::MicAudioCaptor() {
    const AVInputFormat *fmt = av_find_input_format("dshow");
    if (fmt) {
        AVDeviceInfoList *device_list = nullptr;
        if (avdevice_list_input_sources(fmt, nullptr, nullptr, &device_list) >= 0 && device_list) {
            int def_idx = device_list->default_device;

            // 阶段 1: DShow 默认设备
            if (def_idx >= 0 && def_idx < device_list->nb_devices && device_list->devices[def_idx]) {
                m_device_name = "audio=" + std::string(device_list->devices[def_idx]->device_name);
            }

            // 阶段 2: QMediaDevices 匹配（DShow 未提供默认时）
            if (m_device_name.empty() && device_list->nb_devices > 0) {
                QAudioDevice qt_default = QMediaDevices::defaultAudioInput();
                if (!qt_default.isNull()) {
                    QString qt_name = qt_default.description();
                    for (int i = 0; i < device_list->nb_devices; ++i) {
                        if (device_list->devices[i] &&
                            qt_name == QString::fromUtf8(device_list->devices[i]->device_description)) {
                            m_device_name = "audio=" + std::string(device_list->devices[i]->device_name);
                            break;
                        }
                    }
                }
            }

            // 阶段 3: 使用第一个设备保底
            if (m_device_name.empty() && device_list->nb_devices > 0 && device_list->devices[0]) {
                m_device_name = "audio=" + std::string(device_list->devices[0]->device_name);
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