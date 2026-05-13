#include "screencapturesource.h"
#include "screencaptor.h"

ScreenCaptureSource::ScreenCaptureSource(const CaptorConfig &config)
    : VideoSource([&config]() {
        auto captor = std::make_unique<ScreenCaptor>();
        captor->apply_config(config);
        return captor;
    }())
{
    base_width  = static_cast<float>(config.width);
    base_height = static_cast<float>(config.height);
    pos_x = 0.0f;
    pos_y = 0.0f;
    scale_x = 1.0f;
    scale_y = 1.0f;
    visible = true;
    lock_aspect_ratio  = true;
    fixed_aspect_ratio = 16.0f / 9.0f;
}
