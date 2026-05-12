#ifndef OBSIM_AUDIOMANAGER_H
#define OBSIM_AUDIOMANAGER_H

#include <algorithm>
#include <memory>
#include <vector>
#include <atomic>
#include <cmath>
#include <algorithm>


#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QDebug>
#include <QMetaObject>

#include "audiocaptor.h"
#include "systemaudiocaptor.h"
#include "micaudiocaptor.h"

class AudioManager : public QObject {
    Q_OBJECT

public:
    explicit AudioManager(QObject *parent = nullptr);

    ~AudioManager() override;

    // 启动/停止所有音频采集
    void start_all();

    void stop_all();

    // 获取指定音轨的电平（0.0 ~ 1.0）
    float get_system_audio_level() const { return m_system_level.load(); }
    float get_mic_audio_level() const { return m_mic_level.load(); }

private:
    void calculate_level_from_frame(const AVFrame *frame, std::atomic<float> &level_store);

private:
    std::unique_ptr<SystemAudioCaptor> m_system_audio;
    std::unique_ptr<MicAudioCaptor> m_mic_audio;

    std::atomic<float> m_system_level{0.0f};
    std::atomic<float> m_mic_level{0.0f};

    QTimer *m_level_emit_timer = nullptr;

signals:
    void levels_updated(float system_level, float mic_level);
};

#endif
