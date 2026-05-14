#ifndef OBSIM_CAMERACAPTURESOURCE_H
#define OBSIM_CAMERACAPTURESOURCE_H

#include "videosource.h"

class CameraCaptureSource : public VideoSource {
public:
    explicit CameraCaptureSource(std::string device_description = "");
    ~CameraCaptureSource() override = default;
    const char* source_type_name() const override { return "Camera"; }
};

#endif