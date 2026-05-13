#ifndef OBSIM_SYSTEMAUDIOCAPTOR_H
#define OBSIM_SYSTEMAUDIOCAPTOR_H

#include <QDebug>

#include "audiocaptor.h"

struct IMMDeviceEnumerator;
struct IMMDevice;
struct IAudioClient;
struct IAudioCaptureClient;

class SystemAudioCaptor : public AudioCaptor {
public:
    SystemAudioCaptor();
    ~SystemAudioCaptor() override;

protected:
    void init_ctx() override;
    void capture_loop() override;

    const char* get_input_format_name() const override { return "wasapi"; }
    const char* get_device_name() const override { return ""; }
    void setup_options(AVDictionary** opts) override {}

private:
    void cleanup_wasapi();

    IMMDeviceEnumerator* m_enumerator = nullptr;
    IMMDevice* m_device = nullptr;
    IAudioClient* m_audio_client = nullptr;
    IAudioCaptureClient* m_capture_client = nullptr;

    int m_sample_rate = 0;
    int m_channels = 0;
    int m_bytes_per_frame = 0;
    AVSampleFormat m_av_sample_fmt = AV_SAMPLE_FMT_NONE;
};

#endif
