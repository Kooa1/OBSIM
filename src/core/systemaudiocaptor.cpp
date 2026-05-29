#include "systemaudiocaptor.h"

// GUID definitions for WASAPI format subtypes (avoids dependency on ksmedia.h)
static const GUID s_guid_float = {
    0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}
};
static const GUID s_guid_pcm = {
    0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}
};

SystemAudioCaptor::SystemAudioCaptor() {
    init_ctx();
}

SystemAudioCaptor::SystemAudioCaptor(const QString &device_id)
    : m_device_id(device_id) {
    init_ctx();
}

SystemAudioCaptor::~SystemAudioCaptor() {
    if (m_initialized) {
        is_capturing.store(false);
        if (cap_thread.joinable()) {
            cap_thread.join();
        }
    }
    cleanup_wasapi();
}

void SystemAudioCaptor::cleanup_wasapi() {
    if (m_capture_client) { m_capture_client->Release(); m_capture_client = nullptr; }
    if (m_audio_client) { m_audio_client->Release(); m_audio_client = nullptr; }
    if (m_device) { m_device->Release(); m_device = nullptr; }
    if (m_enumerator) { m_enumerator->Release(); m_enumerator = nullptr; }
}

// WASAPI is initialized lazily in capture_loop() on the capture thread
void SystemAudioCaptor::init_ctx() {
    m_initialized = true;
}

static AVSampleFormat wasapi_format_to_av(const WAVEFORMATEX* fmt) {
    if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(fmt);
        if (memcmp(&ext->SubFormat, &s_guid_float, sizeof(GUID)) == 0)
            return AV_SAMPLE_FMT_FLT;
        if (memcmp(&ext->SubFormat, &s_guid_pcm, sizeof(GUID)) == 0) {
            if (fmt->wBitsPerSample == 16) return AV_SAMPLE_FMT_S16;
            if (fmt->wBitsPerSample == 32) return AV_SAMPLE_FMT_S32;
        }
        return AV_SAMPLE_FMT_NONE;
    }
    if (fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
        return AV_SAMPLE_FMT_FLT;
    if (fmt->wFormatTag == WAVE_FORMAT_PCM) {
        if (fmt->wBitsPerSample == 16) return AV_SAMPLE_FMT_S16;
        if (fmt->wBitsPerSample == 32) return AV_SAMPLE_FMT_S32;
    }
    return AV_SAMPLE_FMT_NONE;
}

void SystemAudioCaptor::capture_loop() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool com_ok = SUCCEEDED(hr) || hr == S_FALSE;

    if (!com_ok) {
        return;
    }

    // Create device enumerator
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&m_enumerator));
    if (FAILED(hr)) {
        CoUninitialize();
        return;
    }

    // Get the audio endpoint device
    if (m_device_id.isEmpty()) {
        hr = m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
    } else {
        hr = m_enumerator->GetDevice(
            reinterpret_cast<LPCWSTR>(m_device_id.utf16()), &m_device);
    }
    if (FAILED(hr)) {
        cleanup_wasapi();
        CoUninitialize();
        return;
    }

    // Activate IAudioClient
    hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                            reinterpret_cast<void**>(&m_audio_client));
    if (FAILED(hr)) {
        cleanup_wasapi();
        CoUninitialize();
        return;
    }

    // Get the mix format (device's native format)
    WAVEFORMATEX* mix_fmt = nullptr;
    hr = m_audio_client->GetMixFormat(&mix_fmt);
    if (FAILED(hr) || !mix_fmt) {
        cleanup_wasapi();
        CoUninitialize();
        return;
    }

    m_av_sample_fmt = wasapi_format_to_av(mix_fmt);
    if (m_av_sample_fmt == AV_SAMPLE_FMT_NONE) {
        CoTaskMemFree(mix_fmt);
        cleanup_wasapi();
        CoUninitialize();
        return;
    }

    m_sample_rate = mix_fmt->nSamplesPerSec;
    m_channels = mix_fmt->nChannels;
    m_bytes_per_frame = mix_fmt->nBlockAlign;

    // Initialize audio client with loopback flag
    const REFERENCE_TIME buf_duration = 100000; // 10 ms
    hr = m_audio_client->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        buf_duration, 0, mix_fmt, nullptr);
    if (FAILED(hr)) {
        CoTaskMemFree(mix_fmt);
        cleanup_wasapi();
        CoUninitialize();
        return;
    }

    CoTaskMemFree(mix_fmt);

    // Get capture client for reading loopback data
    hr = m_audio_client->GetService(__uuidof(IAudioCaptureClient),
                                     reinterpret_cast<void**>(&m_capture_client));
    if (FAILED(hr) || !m_capture_client) {
        cleanup_wasapi();
        CoUninitialize();
        return;
    }

    // Start the audio client
    hr = m_audio_client->Start();
    if (FAILED(hr)) {
        cleanup_wasapi();
        CoUninitialize();
        return;
    }

    // ===== Capture loop =====
    while (is_capturing.load()) {
        UINT32 packet_len = 0;
        hr = m_capture_client->GetNextPacketSize(&packet_len);
        if (FAILED(hr)) {
            break;
        }

        if (packet_len == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        BYTE* data = nullptr;
        UINT32 frames_avail = 0;
        DWORD flags = 0;

        hr = m_capture_client->GetBuffer(&data, &frames_avail, &flags, nullptr, nullptr);
        if (FAILED(hr)) {
            break;
        }

                if (frames_avail > 0) {
            AVFramePtr frame(av_frame_alloc(), AVFrameDeleter());
            if (frame) {
                frame->format = m_av_sample_fmt;
                frame->sample_rate = m_sample_rate;
                av_channel_layout_default(&frame->ch_layout, m_channels);
                frame->nb_samples = frames_avail;

                int ret = av_frame_get_buffer(frame.get(), 0);
                if (ret >= 0) {
                    int data_size = frames_avail * m_bytes_per_frame;
                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        memset(frame->data[0], 0, data_size);
                    } else if (data) {
                        memcpy(frame->data[0], data, data_size);
                    }
                    frame->linesize[0] = data_size;

                    push_to_record_queue(frame);

                    queue->push_no_wait(std::move(frame));
                    notify_frame_ready();
                }
            }
        }

        m_capture_client->ReleaseBuffer(frames_avail);
    }

    // Stop
    if (m_audio_client) {
        m_audio_client->Stop();
    }

    cleanup_wasapi();
    CoUninitialize();
}
