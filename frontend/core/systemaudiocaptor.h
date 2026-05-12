#ifndef OBSIM_SYSTEMAUDIOCAPTOR_H
#define OBSIM_SYSTEMAUDIOCAPTOR_H

#include <QDebug>

#include "audiocaptor.h"

class SystemAudioCaptor : public AudioCaptor {
public:
    SystemAudioCaptor();
    ~SystemAudioCaptor() override = default;

protected:
    const char* get_input_format_name() const override;
    const char* get_device_name() const override;
    void setup_options(AVDictionary** opts) override;
};

#endif