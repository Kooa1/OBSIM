#ifndef OBSIM_MICAUDIOCAPTOR_H
#define OBSIM_MICAUDIOCAPTOR_H

#include <QDebug>
#include <string>

#include "base/audiocaptor.h"

/// @brief Microphone audio captor that captures audio from microphone via DShow
class MicAudioCaptor : public AudioCaptor {
public:
    /// @brief Constructor using default microphone device
    MicAudioCaptor();
    /// @brief Constructor with specific device name
    /// @param device_name Microphone device name
    explicit MicAudioCaptor(const std::string &device_name);
    ~MicAudioCaptor() override = default;

    const char* get_device_name() const override;

protected:
    const char* get_input_format_name() const override;
    void setup_options(AVDictionary** opts) override;

private:
    std::string m_device_name; ///< Microphone device name
};

#endif