#include "cameracapturesource.h"
#include "filter/filteredvideocaptor.h"

CameraCaptureSource::CameraCaptureSource(std::string device_description)
    : VideoSource(std::make_unique<FilteredVideoCaptor>(
          std::make_unique<Camera>(std::move(device_description))))
{
    base_width  = 1920.0f;
    base_height = 1080.0f;
    pos_x = 200.0f;
    pos_y = 200.0f;
    scale_x = 0.5f;
    scale_y = 0.5f;
    visible = true;
    lock_aspect_ratio  = true;
    fixed_aspect_ratio = 16.0f / 9.0f;
}