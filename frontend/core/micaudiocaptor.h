#ifndef OBSIM_MICAUDIOCAPTOR_H
#define OBSIM_MICAUDIOCAPTOR_H

#include <QDebug>
#include <string>

#include "audiocaptor.h"

class MicAudioCaptor : public AudioCaptor {
public:
    MicAudioCaptor();
    ~MicAudioCaptor() override = default;

protected:
    const char* get_input_format_name() const override;
    const char* get_device_name() const override;
    void setup_options(AVDictionary** opts) override;

private:
    std::string m_device_name;  // 存储设备名
};

#endif