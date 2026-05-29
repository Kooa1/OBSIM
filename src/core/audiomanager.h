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

/// @brief Manages system audio and microphone capture, providing audio level monitoring and recording queue management.
class AudioManager : public QObject {
    Q_OBJECT

public:
    /// @brief Constructs an AudioManager with default system audio and microphone captors.
    explicit AudioManager(QObject *parent = nullptr);

    /// @brief Constructs an AudioManager with custom system audio and microphone captors.
    explicit AudioManager(
        std::unique_ptr<SystemAudioCaptor> system,
        std::unique_ptr<MicAudioCaptor> mic,
        QObject *parent = nullptr);

    ~AudioManager() override;

    /// @brief Starts all audio capture sources.
    void start_all();

    /// @brief Stops all audio capture sources.
    void stop_all();

    /// @brief Replaces the system audio capture device dynamically.
    void set_system_audio_device(const QString &device_id);
    /// @brief Replaces the microphone capture device dynamically.
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

    float get_system_audio_level() const { return m_system_level.load(); }
    float get_mic_audio_level() const { return m_mic_level.load(); }

    /// @brief Enables recording queues for both system and microphone audio.
    void enable_recording();

    /// @brief Disables and releases recording queues.
    void disable_recording();

    /// @brief Increments the recording reference count and enables recording if it was zero.
    void enable_recording_once();
    /// @brief Decrements the recording reference count and disables recording if it reaches zero.
    void disable_recording_once();

    DataSafeQueue<AVFramePtr> *system_record_queue() const { return m_system_record_queue.get(); }

    DataSafeQueue<AVFramePtr> *mic_record_queue() const { return m_mic_record_queue.get(); }

private:
    void calculate_level_from_frame(const AVFrame *frame, std::atomic<float> &level_store);
    void setup_callbacks();

private:
    std::unique_ptr<SystemAudioCaptor> m_system_audio; ///< System audio capture instance
    std::unique_ptr<MicAudioCaptor> m_mic_audio; ///< Microphone capture instance

    std::atomic<float> m_system_level{0.0f}; ///< Current system audio level (0.0 ~ 1.0)
    std::atomic<float> m_mic_level{0.0f}; ///< Current microphone audio level (0.0 ~ 1.0)

    QTimer *m_level_emit_timer = nullptr; ///< Timer for periodic level emission and decay

    std::unique_ptr<DataSafeQueue<AVFramePtr>> m_system_record_queue; ///< Recording frame queue for system audio
    std::unique_ptr<DataSafeQueue<AVFramePtr>> m_mic_record_queue; ///< Recording frame queue for microphone audio

    QString m_system_device_id; ///< Current system audio device identifier
    std::string m_mic_device_name; ///< Current microphone device name

    std::atomic<int> m_record_ref_count{0}; ///< Reference count for recording activation tracking

signals:
    /// @brief Emitted when audio levels are updated, carrying system and mic levels.
    void levels_updated(float system_level, float mic_level);
};

#endif
