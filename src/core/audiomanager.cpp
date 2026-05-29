#include "audiomanager.h"

AudioManager::AudioManager(QObject *parent)
    : QObject(parent) {
    m_system_audio = std::make_unique<SystemAudioCaptor>();
    m_mic_audio = std::make_unique<MicAudioCaptor>();

    setup_callbacks();
}

AudioManager::AudioManager(
    std::unique_ptr<SystemAudioCaptor> system,
    std::unique_ptr<MicAudioCaptor> mic,
    QObject *parent)
    : QObject(parent)
    , m_system_audio(std::move(system))
    , m_mic_audio(std::move(mic)) {
    setup_callbacks();
}

void AudioManager::setup_callbacks() {
    QPointer<AudioManager> self(this);

    // System audio callback
    m_system_audio->set_frame_ready_callback([self]() {
        if (self) {
            auto frame = self->m_system_audio->try_pop_frame();
            if (frame.has_value()) {
                self->calculate_level_from_frame(frame.value().get(), self->m_system_level);

                QMetaObject::invokeMethod(self, [self]() {
                    if (self) {
                        emit self->levels_updated(
                            self->m_system_level.load(),
                            self->m_mic_level.load()
                        );
                    }
                }, Qt::QueuedConnection);
            }
        }
    });

    // Microphone callback
    m_mic_audio->set_frame_ready_callback([self]() {
        if (self) {
            auto frame = self->m_mic_audio->try_pop_frame();
            if (frame.has_value()) {
                self->calculate_level_from_frame(frame.value().get(), self->m_mic_level);

                QMetaObject::invokeMethod(self, [self]() {
                    if (self) {
                        emit self->levels_updated(
                            self->m_system_level.load(),
                            self->m_mic_level.load()
                        );
                    }
                }, Qt::QueuedConnection);
            }
        }
    });

    // 60 Hz timer for UI refresh and inter-frame decay compensation
    m_level_emit_timer = new QTimer(this);
    connect(m_level_emit_timer, &QTimer::timeout, this, [this]() {
        float mic = m_mic_level.load();
        if (mic > 0.01f) {
            mic *= 0.95f;
            if (mic < 0.005f) mic = 0.0f;
            m_mic_level.store(mic);
        }
        emit levels_updated(m_system_level.load(), m_mic_level.load());
    });
}

// Helper: safely extract sample value based on AVSampleFormat
static float get_sample_from_plane(const uint8_t *plane, int sample_index, AVSampleFormat fmt) {
    switch (fmt) {
        case AV_SAMPLE_FMT_U8: {
            // Unsigned 8-bit: [0, 255] -> [-1.0, 1.0]
            const auto *data = reinterpret_cast<const uint8_t *>(plane);
            return (static_cast<float>(data[sample_index]) - 128.0f) / 128.0f;
        }
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P: {
            // Signed 16-bit: [-32768, 32767] -> [-1.0, 1.0]
            const auto *data = reinterpret_cast<const int16_t *>(plane);
            return static_cast<float>(data[sample_index]) / 32768.0f;
        }
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P: {
            // Signed 32-bit: [-2^31, 2^31-1] -> [-1.0, 1.0]
            const auto *data = reinterpret_cast<const int32_t *>(plane);
            return static_cast<float>(data[sample_index]) / 2147483648.0f;
        }
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP: {
            // Float 32-bit: already in [-1.0, 1.0] range
            const auto *data = reinterpret_cast<const float *>(plane);
            return data[sample_index];
        }
        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_DBLP: {
            // Double 64-bit: already in [-1.0, 1.0] range
            const auto *data = reinterpret_cast<const double *>(plane);
            return static_cast<float>(data[sample_index]);
        }
        case AV_SAMPLE_FMT_S64:
        case AV_SAMPLE_FMT_S64P: {
            // Signed 64-bit
            const auto *data = reinterpret_cast<const int64_t *>(plane);
            return static_cast<float>(data[sample_index]) / 9223372036854775808.0f;
        }
        default:
            return 0.0f;
    }
}

AudioManager::~AudioManager() {
    m_level_emit_timer->stop();
    if (m_system_audio) m_system_audio->stop();
    if (m_mic_audio) m_mic_audio->stop();
}

void AudioManager::start_all() {
    m_system_level.store(0.0f);
    m_mic_level.store(0.0f);

    if (m_system_audio) m_system_audio->start();
    if (m_mic_audio) m_mic_audio->start();

    if ((m_system_audio && m_system_audio->is_running()) ||
        (m_mic_audio && m_mic_audio->is_running())) {
        m_level_emit_timer->start(16);

        emit levels_updated(0.0f, 0.0f);
        }
}

void AudioManager::stop_all() {
    m_level_emit_timer->stop();
    if (m_system_audio) m_system_audio->stop();
    if (m_mic_audio) m_mic_audio->stop();
}

void AudioManager::set_system_audio_device(const QString &device_id) {
    bool was_running = m_system_audio && m_system_audio->is_running();

    if (m_system_audio) {
        m_system_audio->stop();
    }

    m_system_device_id = device_id;

    auto new_captor = std::make_unique<SystemAudioCaptor>(device_id);

    if (m_system_record_queue) {
        new_captor->set_record_queue(m_system_record_queue.get());
    }

    QPointer<AudioManager> self(this);
    new_captor->set_frame_ready_callback([self]() {
        if (self) {
            auto frame = self->m_system_audio->try_pop_frame();
            if (frame.has_value()) {
                self->calculate_level_from_frame(frame.value().get(), self->m_system_level);
                QMetaObject::invokeMethod(self, [self]() {
                    if (self) emit self->levels_updated(
                        self->m_system_level.load(), self->m_mic_level.load());
                }, Qt::QueuedConnection);
            }
        }
    });

    m_system_audio = std::move(new_captor);

    if (was_running) {
        m_system_level.store(0.0f);
        m_system_audio->start();
        if (!m_level_emit_timer->isActive()) {
            m_level_emit_timer->start(16);
        }
    }
}

void AudioManager::set_mic_audio_device(const std::string &device_name) {
    bool was_running = m_mic_audio && m_mic_audio->is_running();

    if (m_mic_audio) {
        m_mic_audio->stop();
    }

    m_mic_device_name = device_name;

    auto new_captor = std::make_unique<MicAudioCaptor>(device_name);

    if (m_mic_record_queue) {
        new_captor->set_record_queue(m_mic_record_queue.get());
    }

    QPointer<AudioManager> self(this);
    new_captor->set_frame_ready_callback([self]() {
        if (self) {
            auto frame = self->m_mic_audio->try_pop_frame();
            if (frame.has_value()) {
                self->calculate_level_from_frame(frame.value().get(), self->m_mic_level);
                QMetaObject::invokeMethod(self, [self]() {
                    if (self) emit self->levels_updated(
                        self->m_system_level.load(), self->m_mic_level.load());
                }, Qt::QueuedConnection);
            }
        }
    });

    m_mic_audio = std::move(new_captor);

    if (was_running) {
        m_mic_level.store(0.0f);
        m_mic_audio->start();
        if (!m_level_emit_timer->isActive()) {
            m_level_emit_timer->start(16);
        }
    }
}

void AudioManager::enable_recording() {
    m_system_record_queue = std::make_unique<DataSafeQueue<AVFramePtr>>(120);
    m_mic_record_queue = std::make_unique<DataSafeQueue<AVFramePtr>>(120);
    if (m_system_audio) m_system_audio->set_record_queue(m_system_record_queue.get());
    if (m_mic_audio) m_mic_audio->set_record_queue(m_mic_record_queue.get());
}

void AudioManager::disable_recording() {
    if (m_system_audio) m_system_audio->set_record_queue(nullptr);
    if (m_mic_audio) m_mic_audio->set_record_queue(nullptr);
    m_system_record_queue.reset();
    m_mic_record_queue.reset();
}

void AudioManager::enable_recording_once() {
    int prev = m_record_ref_count.fetch_add(1);
    if (prev == 0) {
        enable_recording();
    }
}

void AudioManager::disable_recording_once() {
    int prev = m_record_ref_count.fetch_sub(1);
    if (prev <= 1) {
        disable_recording();
    }
}

void AudioManager::calculate_level_from_frame(const AVFrame* frame, std::atomic<float>& level_store) {
    if (!frame || frame->nb_samples <= 0) {
        float current = level_store.load();
        float decayed = current * 0.92f;
        if (decayed < 0.01f) decayed = 0.0f;
        level_store.store(decayed);
        return;
    }

    float sum = 0.0f;
    int channels = frame->ch_layout.nb_channels;
    int samples = frame->nb_samples;

    bool is_planar = av_sample_fmt_is_planar(static_cast<AVSampleFormat>(frame->format));

    int sample_count = 0;
    for (int ch = 0; ch < channels; ++ch) {
        int stride = is_planar ? 1 : channels;
        const uint8_t* plane_data = is_planar ? frame->data[ch] : frame->data[0];

        for (int i = 0; i < samples; ++i) {
            int sample_index = is_planar ? i : i * stride + ch;
            float sample = get_sample_from_plane(plane_data, sample_index,
                         static_cast<AVSampleFormat>(frame->format));
            sum += sample * sample;
            sample_count++;
        }
    }

    if (sample_count == 0) return;

    float rms = std::sqrt(sum / sample_count);

    float current = level_store.load();
    float smoothed;
    bool is_mic = (&level_store == &m_mic_level);

    if (is_mic) {
        float level;
        if (rms < 0.008f) {
            level = 0.0f;
        } else {
            level = std::min(1.0f, std::sqrt(rms) * 2.0f);
        }
        if (level < 0.005f) level = 0.0f;

        if (level > current) {
            smoothed = current * 0.3f + level * 0.7f;
        } else {
            smoothed = current * 0.8f + level * 0.2f;
        }
    } else {
        float level = std::min(1.0f, std::sqrt(rms) * 1.5f);
        if (level < 0.005f) level = 0.0f;

        if (level > current) {
            smoothed = current * 0.3f + level * 0.7f;
        } else {
            smoothed = current * 0.8f + level * 0.2f;
        }
    }

    level_store.store(smoothed);
}
