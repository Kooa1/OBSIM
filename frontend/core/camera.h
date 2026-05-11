//
// Created by 66 on 2025/12/18.
//

#ifndef MONITORSERVER_CAPTOR_H
#define MONITORSERVER_CAPTOR_H

extern "C" {
#include <libavutil/log.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include <iostream>
#include <fstream>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <optional>

#include "../utils/ffmpegfactory.h"
#include "../utils/av_err2str_cxx.h"
#include "../utils/datasafequeue.h"

using std::cout;

class Camera {
public:
    using FrameReadyCallback = std::function<void()>;

    Camera();
    ~Camera();

    void start();
    void stop();

    void set_frame_ready_callback(FrameReadyCallback callback);
    std::optional<AVFramePtr> try_pop_frame();
    bool is_running() const { return is_capturing.load(); }

private:
    void init_ctx();                     // FFmpeg 初始化
    void init_sws_ctx();                 // sws 上下文初始化
    void capture_loop();                 // 采集线程主循环

    void receive_frame0(AVPacketPtr obj_packet);   // 需要格式转换
    void receive_frame1(AVPacketPtr obj_packet);   // 不需要格式转换

    void notify_frame_ready();

private:
    FrameReadyCallback frame_ready_callback;
    std::mutex callback_mutex;

    std::thread cap_thread;
    std::atomic_bool is_capturing{false};

    std::unique_ptr<DataSafeQueue<AVFramePtr>> queue;

    std::function<void(AVPacketPtr)> decode_func;
    enum AVPixelFormat target_pixel_format = AV_PIX_FMT_BGRA;
    bool change_fmt = false;

    const AVInputFormat* av_input_format = nullptr;
    const AVCodec*       av_coder = nullptr;
    AVFormatContextPtr   av_format_context = nullptr;
    AVCodecContextPtr    av_codec_context = nullptr;
    AVDictionary*        options = nullptr;
    AVStreamPtr          video_stream = nullptr;
    SwsContextPtr         sws_context = nullptr;

    std::string device_name;
    int video_index = -1;
};

#endif //MONITORSERVER_CAPTOR_H