#ifndef OBSIM_SCREENCAPTURESOURCE_H
#define OBSIM_SCREENCAPTURESOURCE_H

#include "videosource.h"
#include "screencaptor.h"

/// @brief Screen capture source that captures the desktop/display content
class ScreenCaptureSource : public VideoSource {
public:
    /// @brief Constructor with captor configuration
    /// @param config Capture region and display configuration
    explicit ScreenCaptureSource(const CaptorConfig &config);
    ~ScreenCaptureSource() override = default;
    const char* source_type_name() const override { return "Screen Capture"; }

    CaptorConfig captor_config() const { return m_config; }

private:
    CaptorConfig m_config; ///< Capture configuration
};

#endif