#ifndef MONITORSERVER_CAPTOR_H
#define MONITORSERVER_CAPTOR_H

#include "base/videocaptor.h"

/// @brief Camera capture implementation using dshow (Windows) or avfoundation (macOS).
class Camera : public VideoCaptor {
public:
    /// @brief Constructs a Camera with an optional device description.
    explicit Camera(std::string device_description = "");
    ~Camera() override = default;

protected:
    const char* get_input_format_name() const override;
    const char* get_device_name() const override;
    void setup_options(AVDictionary** opts) override;

private:
    std::string m_device_description; ///< User-readable device description
    std::string m_device_url; ///< FFmpeg device URL (e.g. "video=USB Video Device")
};

#endif