#ifndef OBSIM_SCREENCAPTURESOURCE_H
#define OBSIM_SCREENCAPTURESOURCE_H

#include "videosource.h"
#include "screencaptor.h"

class ScreenCaptureSource : public VideoSource {
public:
    explicit ScreenCaptureSource(const CaptorConfig &config);
    ~ScreenCaptureSource() override = default;
    const char* source_type_name() const override { return "Screen Capture"; }

    CaptorConfig captor_config() const { return m_config; }

private:
    CaptorConfig m_config;
};

#endif