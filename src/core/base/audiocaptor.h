#ifndef OBSIM_AUDIOCAPTOR_H
#define OBSIM_AUDIOCAPTOR_H

#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <optional>
#include <mutex>
#include <iostream>
#include <chrono>

#include <QDebug>

#include "../../utils/ffmpegfactory.h"
#include "../../utils/datasafequeue.h"
#include "../../utils/av_err2str_cxx.h"

extern "C" {
#include <libavutil/log.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

/// @brief Base class for audio capture from various input devices (e.g. microphone, system audio).
/// Manages FFmpeg decoding pipeline, audio frame queue, and optional pass-through to a recording queue.
class AudioCaptor {
public:
    using FrameReadyCallback = std::function<void()>;

    AudioCaptor() = default;

    virtual ~AudioCaptor();

    void start();

    void stop();

    void set_frame_ready_callback(FrameReadyCallback callback);

    std::optional<AVFramePtr> try_pop_frame();

    bool is_running() const { return is_capturing.load(); }

    void set_record_queue(DataSafeQueue<AVFramePtr> *q) { record_queue = q; }

protected:
    virtual void init_ctx();

    virtual void capture_loop();

    void receive_frame(AVPacketPtr packet);

    void notify_frame_ready();

    /// @brief Pushes a frame to the recording queue if available.
    void push_to_record_queue(const AVFramePtr &frame);

    /// @brief Returns the FFmpeg input format name (e.g. "dshow", "alsa").
    virtual const char *get_input_format_name() const = 0;
    /// @brief Returns the device name for audio input.
    virtual const char *get_device_name() const = 0;
    /// @brief Sets up FFmpeg options before opening the audio input device.
    virtual void setup_options(AVDictionary **opts) {
    }

    bool m_initialized = false;                         ///< Whether the audio capture context is initialized.
    std::unique_ptr<DataSafeQueue<AVFramePtr>> queue;   ///< Frame queue for decoded audio frames.
    DataSafeQueue<AVFramePtr> *record_queue = nullptr;  ///< Optional queue for recording pass-through.
    std::atomic_bool is_capturing{false};               ///< Whether capture is active.
    std::thread cap_thread;                             ///< Capture thread.
    FrameReadyCallback frame_ready_callback;             ///< Callback invoked when a new frame is ready.
    std::mutex callback_mutex;                          ///< Mutex protecting frame_ready_callback.

private:

    std::function<void(AVPacketPtr)> decode_func;       ///< Decode dispatch function.

    const AVInputFormat *av_input_format = nullptr;     ///< FFmpeg input format.
    const AVCodec *av_decoder = nullptr;                ///< FFmpeg audio decoder.
    AVFormatContextPtr av_format_context = nullptr;      ///< FFmpeg format context.
    AVCodecContextPtr av_codec_context = nullptr;        ///< FFmpeg codec context.
    AVDictionary *options = nullptr;                    ///< FFmpeg options dictionary.
    AVStreamPtr audio_stream = nullptr;                 ///< Audio stream reference.
    int audio_index = -1;                               ///< Index of the audio stream.
};

#endif