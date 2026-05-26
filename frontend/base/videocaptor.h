//
// Created by 66 on 2026/5/11.
//

#ifndef OBSIM_VIDEOCAPTOR_H
#define OBSIM_VIDEOCAPTOR_H

#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <optional>
#include <iostream>
#include <chrono>

#include "../utils/ffmpegfactory.h"
#include "../utils/datasafequeue.h"
#include "../utils/av_err2str_cxx.h"

extern "C" {
#include <libavutil/log.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

struct CaptorConfig {
    int offset_x = 0;
    int offset_y = 0;
    int width = 0;
    int height = 0;
};

class VideoCaptor {
public:
    using FrameReadyCallback = std::function<void()>;

    VideoCaptor() = default;
    virtual ~VideoCaptor();

    void start();
    void stop();
    void pause();
    void resume();
    void set_frame_ready_callback(FrameReadyCallback callback);
    std::optional<AVFramePtr> try_pop_frame();
    bool is_running() const { return is_capturing.load(); }

protected:
    virtual void apply_config(const CaptorConfig &config);
    virtual void init_ctx();
    void init_sws_ctx();
    void capture_loop();
    void receive_frame0(AVPacketPtr packet);
    void receive_frame1(AVPacketPtr packet);
    void notify_frame_ready();

    // ===== 子类必须实现 =====
    virtual const char* get_input_format_name() const = 0;
    virtual const char* get_device_name() const = 0;
    virtual void setup_options(AVDictionary** opts) {}  // 可选的选项设置

    // ===== 成员变量 =====
    std::unique_ptr<DataSafeQueue<AVFramePtr>> queue;
    std::atomic_bool is_capturing{false};
    std::atomic_bool is_paused_{false};
    std::thread cap_thread;
    FrameReadyCallback frame_ready_callback;
    std::mutex callback_mutex;

    std::function<void(AVPacketPtr)> decode_func;
    enum AVPixelFormat target_pixel_format = AV_PIX_FMT_BGRA;
    bool change_fmt = false;

    CaptorConfig m_config;

    const AVInputFormat* av_input_format = nullptr;
    const AVCodec* av_decoder = nullptr;
    AVFormatContextPtr av_format_context = nullptr;
    AVCodecContextPtr av_codec_context = nullptr;
    AVDictionary* options = nullptr;
    AVStreamPtr video_stream = nullptr;
    SwsContextPtr sws_context = nullptr;
    int video_index = -1;
};

#endif