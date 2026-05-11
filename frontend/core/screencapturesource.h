#ifndef OBSIM_SCREENCAPTURESOURCE_H
#define OBSIM_SCREENCAPTURESOURCE_H

#include "videosource.h"

class ScreenCaptureSource : public VideoSource {
public:
    explicit ScreenCaptureSource(int screen_index);
    ~ScreenCaptureSource() override = default;
    const char* source_type_name() const override { return "Screen Capture"; }
};

#endif