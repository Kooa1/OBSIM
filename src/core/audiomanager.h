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

#include "base/audiocaptor.h"
#include "systemaudiocaptor.h"
#include "micaudiocaptor.h"
#include "../utils/datasafequeue.h"

class AudioManager : public QObject {
    Q_OBJECT

public:
    explicit AudioManager(QObject *parent = nullptr);

    explicit AudioManager(
        std::unique_ptr<SystemAudioCaptor> system,
        std::unique_ptr<MicAudioCaptor> mic,
        QObject *parent = nullptr);

    ~AudioManager() override;

    // 启动/停止所有音频采集
    void start_all();

    void stop_all();

    // 动态更换音频采集设备
    void set_system_audio_device(const QString &device_id);
    void set_mic_audio_device(const std::string &device_name);

    QString current_system_device_id() const {
        if (!m_system_device_id.isEmpty()) return m_system_device_id;
        if (m_system_audio) return m_system_audio->device_id();
        return {};
    }
    std::string current_mic_device_name() const {
        if (!m_mic_device_name.empty()) return m_mic_device_name;
        if (m_mic_audio) {
            const char *dev = m_mic_audio->get_device_name();
            if (dev) {
                std::string s(dev);
                auto pos = s.find("audio=");
                if (pos == 0) return s.substr(6);
                return s;
            }
        }
        return {};
    }

    // 获取指定音轨的电平（0.0 ~ 1.0）
    float get_system_audio_level() const { return m_system_level.load(); }
    float get_mic_audio_level() const { return m_mic_level.load(); }

    // 录音队列管理
    void enable_recording();

    void disable_recording();

    void enable_recording_once();
    void disable_recording_once();

    DataSafeQueue<AVFramePtr> *system_record_queue() const { return m_system_record_queue.get(); }

    DataSafeQueue<AVFramePtr> *mic_record_queue() const { return m_mic_record_queue.get(); }

private:
    void calculate_level_from_frame(const AVFrame *frame, std::atomic<float> &level_store);
    void setup_callbacks();

private:
    std::unique_ptr<SystemAudioCaptor> m_system_audio;
    std::unique_ptr<MicAudioCaptor> m_mic_audio;

    std::atomic<float> m_system_level{0.0f};
    std::atomic<float> m_mic_level{0.0f};

    QTimer *m_level_emit_timer = nullptr;

    std::unique_ptr<DataSafeQueue<AVFramePtr>> m_system_record_queue;
    std::unique_ptr<DataSafeQueue<AVFramePtr>> m_mic_record_queue;

    QString m_system_device_id;
    std::string m_mic_device_name;

    std::atomic<int> m_record_ref_count{0};

signals:
    void levels_updated(float system_level, float mic_level);
};

#endif
