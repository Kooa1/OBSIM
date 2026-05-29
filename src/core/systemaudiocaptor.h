#ifndef OBSIM_SYSTEMAUDIOCAPTOR_H
#define OBSIM_SYSTEMAUDIOCAPTOR_H

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#include <QDebug>

#include "base/audiocaptor.h"

struct IMMDeviceEnumerator;
struct IMMDevice;
struct IAudioClient;
struct IAudioCaptureClient;

/// @brief System audio captor that captures loopback audio from WASAPI output devices
class SystemAudioCaptor : public AudioCaptor {
public:
    /// @brief Constructor using default audio endpoint
    SystemAudioCaptor();
    /// @brief Constructor with specific device ID
    /// @param device_id WASAPI device identifier string
    explicit SystemAudioCaptor(const QString &device_id);
    ~SystemAudioCaptor() override;

    const QString& device_id() const { return m_device_id; }

protected:
    /// @brief Initialize WASAPI capture context (lazy init on capture thread)
    void init_ctx() override;
    /// @brief Main capture loop running on a separate thread
    void capture_loop() override;

    const char* get_input_format_name() const override { return "wasapi"; }
    const char* get_device_name() const override { return ""; }
    void setup_options(AVDictionary** opts) override {}

private:
    /// @brief Release all WASAPI COM interfaces
    void cleanup_wasapi();

    QString m_device_id;                  ///< WASAPI device identifier
    IMMDeviceEnumerator* m_enumerator = nullptr;      ///< WASAPI device enumerator COM interface
    IMMDevice* m_device = nullptr;                    ///< WASAPI endpoint device COM interface
    IAudioClient* m_audio_client = nullptr;           ///< WASAPI audio client COM interface
    IAudioCaptureClient* m_capture_client = nullptr;   ///< WASAPI capture client COM interface

    int m_sample_rate = 0;                ///< Audio sample rate in Hz
    int m_channels = 0;                   ///< Number of audio channels
    int m_bytes_per_frame = 0;            ///< Bytes per audio frame
    AVSampleFormat m_av_sample_fmt = AV_SAMPLE_FMT_NONE; ///< FFmpeg sample format
};

#endif