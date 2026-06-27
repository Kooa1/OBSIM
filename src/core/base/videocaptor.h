#ifndef OBSIM_VIDEOCAPTOR_H
#define OBSIM_VIDEOCAPTOR_H

#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <optional>
#include <iostream>
#include <chrono>

#include "../../utils/ffmpegfactory.h"
#include "../../utils/datasafequeue.h"
#include "../../utils/av_err2str_cxx.h"

extern "C" {
#include <libavutil/log.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

/// @brief Configuration for video capture region and offset.
struct CaptorConfig {
    int offset_x = 0;
    int offset_y = 0;
    int width = 0;
    int height = 0;
    bool capture_cursor = true;
};

/// @brief Base class for video capture from various input sources (e.g. screen, camera).
/// Manages FFmpeg decoding pipeline, frame queue, and capture lifecycle.
class VideoCaptor {
public:
    using FrameReadyCallback = std::function<void()>;

    VideoCaptor() = default;
    virtual ~VideoCaptor();

    virtual void start();
    virtual void stop();
    virtual void pause();
    virtual void resume();
    void set_frame_ready_callback(FrameReadyCallback callback);
    std::optional<AVFramePtr> try_pop_frame();
    bool is_running() const { return is_capturing.load(); }

    /// @brief Returns the FFmpeg input format name (e.g. "gdigrab", "dshow").
    virtual const char* get_input_format_name() const = 0;
    /// @brief Returns the device name or URL for the input source.
    virtual const char* get_device_name() const = 0;
    /// @brief Sets up FFmpeg options before opening the input device.
    virtual void setup_options(AVDictionary** opts) {}

protected:
    virtual void apply_config(const CaptorConfig &config);
    virtual void init_ctx();
    void init_sws_ctx();
    void capture_loop();
    virtual void receive_frame0(AVPacketPtr packet);
    virtual void receive_frame1(AVPacketPtr packet);
    virtual void push_frame(AVFramePtr frame);
    void notify_frame_ready();

    std::unique_ptr<DataSafeQueue<AVFramePtr>> queue;   ///< Frame queue for decoded video frames.
    std::atomic_bool is_capturing{false};               ///< Whether capture is active.
    std::atomic_bool is_paused_{false};                 ///< Whether capture is paused.
    std::thread cap_thread;                             ///< Capture thread.
    FrameReadyCallback frame_ready_callback;             ///< Callback invoked when a new frame is ready.
    std::mutex callback_mutex;                          ///< Mutex protecting frame_ready_callback.

    std::function<void(AVPacketPtr)> decode_func;       ///< Selected decode function based on pixel format.
    enum AVPixelFormat target_pixel_format = AV_PIX_FMT_BGRA;  ///< Target pixel format for output.
    bool change_fmt = false;                            ///< Whether pixel format conversion is needed.

    CaptorConfig m_config;                              ///< Capture region configuration.

    const AVInputFormat* av_input_format = nullptr;     ///< FFmpeg input format.
    const AVCodec* av_decoder = nullptr;                ///< FFmpeg video decoder.
    AVFormatContextPtr av_format_context = nullptr;      ///< FFmpeg format context.
    AVCodecContextPtr av_codec_context = nullptr;        ///< FFmpeg codec context.
    AVDictionary* options = nullptr;                    ///< FFmpeg options dictionary.
    AVStreamPtr video_stream = nullptr;                 ///< Video stream reference.
    SwsContextPtr sws_context = nullptr;                ///< SWS scaling context for pixel format conversion.
    int video_index = -1;                               ///< Index of the video stream.
};

#endif