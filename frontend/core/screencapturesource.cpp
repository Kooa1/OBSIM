#include "screencapturesource.h"
#include "screencaptor.h"

ScreenCaptureSource::ScreenCaptureSource(int screen_index)
    : VideoSource(std::make_unique<ScreenCaptor>())
{
    base_width  = 1920.0f;
    base_height = 1080.0f;
    pos_x = 0.0f;
    pos_y = 0.0f;
    scale_x = 1.0f;
    scale_y = 1.0f;
    visible = true;
    lock_aspect_ratio  = true;
    fixed_aspect_ratio = 16.0f / 9.0f;
}