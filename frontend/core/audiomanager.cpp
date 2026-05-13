#include "audiomanager.h"

AudioManager::AudioManager(QObject *parent)
    : QObject(parent) {
    m_system_audio = std::make_unique<SystemAudioCaptor>();
    m_mic_audio = std::make_unique<MicAudioCaptor>();

    QPointer<AudioManager> self(this);

    // ✅ 系统音频回调
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

    // ✅ 麦克风回调
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

    // 60Hz 定时器：UI 刷新 + 帧间过渡衰减（补偿 dshow 低帧率，消除阶梯感）
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

// 辅助函数：根据 AVSampleFormat 安全地提取样本值
static float get_sample_from_plane(const uint8_t *plane, int sample_index, AVSampleFormat fmt) {
    switch (fmt) {
        case AV_SAMPLE_FMT_U8: {
            // 无符号 8 位：[0, 255] -> [-1.0, 1.0]
            const auto *data = reinterpret_cast<const uint8_t *>(plane);
            return (static_cast<float>(data[sample_index]) - 128.0f) / 128.0f;
        }
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P: {
            // 有符号 16 位：[-32768, 32767] -> [-1.0, 1.0]
            const auto *data = reinterpret_cast<const int16_t *>(plane);
            return static_cast<float>(data[sample_index]) / 32768.0f;
        }
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P: {
            // 有符号 32 位：[-2^31, 2^31-1] -> [-1.0, 1.0]
            const auto *data = reinterpret_cast<const int32_t *>(plane);
            return static_cast<float>(data[sample_index]) / 2147483648.0f;
        }
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP: {
            // 浮点 32 位：已经是 [-1.0, 1.0] 范围
            const auto *data = reinterpret_cast<const float *>(plane);
            return data[sample_index];
        }
        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_DBLP: {
            // 双精度浮点 64 位：已经是 [-1.0, 1.0] 范围
            const auto *data = reinterpret_cast<const double *>(plane);
            return static_cast<float>(data[sample_index]);
        }
        case AV_SAMPLE_FMT_S64:
        case AV_SAMPLE_FMT_S64P: {
            // 有符号 64 位
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
    // 重置电平值
    m_system_level.store(0.0f);
    m_mic_level.store(0.0f);

    if (m_system_audio) m_system_audio->start();
    if (m_mic_audio) m_mic_audio->start();

    if ((m_system_audio && m_system_audio->is_running()) ||
        (m_mic_audio && m_mic_audio->is_running())) {
        m_level_emit_timer->start(16); // 改为 60Hz（16ms）
        qDebug() << "AudioManager: 启动电平更新定时器";

        // 立即发射初始电平值
        emit levels_updated(0.0f, 0.0f);
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
        // ===== 麦克风（完全参考系统电平代码结构） =====
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
        // ===== 系统音频 =====
        float level = std::min(1.0f, std::sqrt(rms) * 1.5f);
        if (level < 0.005f) level = 0.0f;

        if (level > current) {
            smoothed = current * 0.3f + level * 0.7f;
        } else {
            smoothed = current * 0.8f + level * 0.2f;
        }
    }

    level_store.store(smoothed);

    static int sys_counter = 0;
    static int mic_counter = 0;

    if (&level_store == &m_system_level) {
        if (++sys_counter <= 5) {
            qDebug() << "AudioManager[SYS] rms:" << rms << "level:" << smoothed;
        }
    } else {
        if (++mic_counter <= 5) {
            qDebug() << "AudioManager[MIC] rms:" << rms << "level:" << smoothed;
        }
    }
}
