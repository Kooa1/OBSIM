#ifndef OBSIM_MICAUDIOCAPTOR_H
#define OBSIM_MICAUDIOCAPTOR_H

#include <QDebug>

#include "audiocaptor.h"

class MicAudioCaptor : public AudioCaptor {
public:
    MicAudioCaptor();
    ~MicAudioCaptor() override = default;

protected:
    const char* get_input_format_name() const override;
    const char* get_device_name() const override;
    void setup_options(AVDictionary** opts) override;
};

#endif