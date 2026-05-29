#ifndef OBSIM_CAMERACAPTURESOURCE_H
#define OBSIM_CAMERACAPTURESOURCE_H

#include "videosource.h"
#include "camera.h"

/// @brief Camera capture source that captures video from a physical camera device
class CameraCaptureSource : public VideoSource {
public:
    /// @brief Constructor with optional device description
    /// @param device_description Device identifier string for camera selection
    explicit CameraCaptureSource(std::string device_description = "");
    ~CameraCaptureSource() override = default;
    const char* source_type_name() const override { return "Camera"; }
};

#endif