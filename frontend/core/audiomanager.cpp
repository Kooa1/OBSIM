#include "audiomanager.h"

AudioManager::AudioManager(QObject* parent)
    : QObject(parent) {

    m_system_audio = std::make_unique<SystemAudioCaptor>();
    m_mic_audio = std::make_unique<MicAudioCaptor>();

    QPointer<AudioManager> self(this);

    m_system_audio->set_frame_ready_callback([self]() {
        if (self) {
            auto frame = self->m_system_audio->try_pop_frame();
            if (frame.has_value()) {
                self->calculate_level_from_frame(frame.value().get(), self->m_system_level);
            }
        }
    });

    m_mic_audio->set_frame_ready_callback([self]() {
        if (self) {
            auto frame = self->m_mic_audio->try_pop_frame();
            if (frame.has_value()) {
                self->calculate_level_from_frame(frame.value().get(), self->m_mic_level);

                // 处理完一帧后直接通知 UI（采集线程中发射信号）
                // Qt 自动将信号投递到主线程（QPointer 的接收者）
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

    // 定时器作为兜底（即使没有新帧也定期更新 UI）
    m_level_emit_timer = new QTimer(this);
    connect(m_level_emit_timer, &QTimer::timeout, this, [this]() {
        emit levels_updated(m_system_level.load(), m_mic_level.load());
    });
}

AudioManager::~AudioManager() {
    m_level_emit_timer->stop();
    if (m_system_audio) m_system_audio->stop();
    if (m_mic_audio) m_mic_audio->stop();
}

void AudioManager::start_all() {
    if (m_system_audio) m_system_audio->start();
    if (m_mic_audio) m_mic_audio->start();

    if ((m_system_audio && m_system_audio->is_running()) ||
        (m_mic_audio && m_mic_audio->is_running())) {
        m_level_emit_timer->start(16);  // 改为 60Hz（16ms）
        qDebug() << "AudioManager: 启动电平更新定时器";
        } else {
            qDebug() << "AudioManager: 无可用音频设备，不启动定时器";
        }
}

void AudioManager::stop_all() {
    m_level_emit_timer->stop();
    if (m_system_audio) m_system_audio->stop();
    if (m_mic_audio) m_mic_audio->stop();
}

void AudioManager::calculate_level_from_frame(const AVFrame* frame, std::atomic<float>& level_store) {
    if (!frame || frame->nb_samples <= 0) {
        float current = level_store.load();
        level_store.store(current * 0.8f);  // 无信号时更快衰减
        return;
    }

    float sum = 0.0f;
    int channels = frame->ch_layout.nb_channels;
    int samples = frame->nb_samples;

    // 使用双声道平面格式数据
    int16_t* data = reinterpret_cast<int16_t*>(frame->data[0]);
    for (int i = 0; i < samples * channels; ++i) {
        float sample = data[i] / 32768.0f;
        sum += sample * sample;
    }

    float rms = std::sqrt(sum / (samples * channels));
    if (rms < 1e-10f) rms = 1e-10f;

    float db = 20.0f * std::log10(rms);
    float level = std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);

    // 改为轻平滑：0.5 / 0.5，即同时有上一帧和当前帧的权重
    float current = level_store.load();
    float smoothed = current * 0.4f + level * 0.6f;
    level_store.store(smoothed);

    static int debug_counter = 0;
    if (++debug_counter % 30 == 0) {
        qDebug() << "AudioManager: format:" << frame->format
                 << "samples:" << samples
                 << "rms:" << rms
                 << "db:" << db
                 << "level:" << smoothed;
    }
}