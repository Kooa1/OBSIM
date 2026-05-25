//
// Created by 66 on 2025/12/18.
//

#ifndef MONITORSERVER_CAPTOR_H
#define MONITORSERVER_CAPTOR_H

#include "../base/videocaptor.h"

class Camera : public VideoCaptor {
public:
    explicit Camera(std::string device_description = "");
    ~Camera() override = default;

protected:
    const char* get_input_format_name() const override;
    const char* get_device_name() const override;
    void setup_options(AVDictionary** opts) override;

private:
    std::string m_device_description;
    std::string m_device_url;
};

#endif