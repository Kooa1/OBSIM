//
// Created by 66 on 2026/4/26.
//

#ifndef OBSIM_CAPTOR_H
#define OBSIM_CAPTOR_H

#include "videocaptor.h"

class ScreenCaptor : public VideoCaptor {
public:
    ScreenCaptor();
    ~ScreenCaptor() override = default;

protected:
    const char* get_input_format_name() const override { return "gdigrab"; }
    const char* get_device_name() const override { return "desktop"; }
    void setup_options(AVDictionary** opts) override {
        av_dict_set(opts, "framerate", "60", 0);
        av_dict_set(opts, "video_size", "1920x1080", 0);
    }
};

#endif