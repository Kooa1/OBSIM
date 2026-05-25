#ifndef OBSIM_CAPTOR_H
#define OBSIM_CAPTOR_H

#include "../base/videocaptor.h"

class ScreenCaptor : public VideoCaptor {
public:
    ScreenCaptor();
    ~ScreenCaptor() override = default;
    void apply_config(const CaptorConfig &config) override;

protected:
    const char* get_input_format_name() const override { return "gdigrab"; }
    const char* get_device_name() const override { return "desktop"; }
    void setup_options(AVDictionary** opts) override;
};

#endif
