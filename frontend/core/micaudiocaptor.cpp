#include "micaudiocaptor.h"

MicAudioCaptor::MicAudioCaptor() {
    // 尝试常见的麦克风设备名称
    const char* test_devices[] = {
        "audio=麦克风 (USB Audio Device)",
        "audio=麦克风 (Realtek Audio)",
        "audio=麦克风 (HD Audio)",
        "audio=Microphone (Realtek Audio)",
        "audio=Microphone (USB Audio Device)"
    };

    bool found = false;
    for (const char* device : test_devices) {
        // 这里可以添加设备检测逻辑
        // 简化版本：直接使用第一个
        m_device_name = device;
        found = true;
        break;
    }

    if (!found) {
        m_device_name = "audio=麦克风";  // 默认设备名
    }

    init_ctx();
    qDebug() << "MicAudioCaptor initialized with device:" << m_device_name << m_initialized;
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