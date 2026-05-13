#ifndef OBSIM_SCREENCAPTURESOURCE_H
#define OBSIM_SCREENCAPTURESOURCE_H

#include "videosource.h"

class ScreenCaptureSource : public VideoSource {
public:
    explicit ScreenCaptureSource(const CaptorConfig &config);
    ~ScreenCaptureSource() override = default;
    const char* source_type_name() const override { return "Screen Capture"; }
};

#endif