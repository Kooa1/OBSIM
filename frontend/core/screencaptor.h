//
// Created by 66 on 2026/4/26.
//

#ifndef OBSIM_CAPTOR_H
#define OBSIM_CAPTOR_H

#include <iostream>
#include <atomic>
#include <functional>

#include "../utils/ffmpegfactory.h"
#include "../utils/displaymanager.h"
#include "../utils/datasafequeue.h"

extern "C" {
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
}

using std::cout;

class ScreenCaptor {
public:
    using FrameReadyCallback = std::function<void()>;

    explicit ScreenCaptor();

    ~ScreenCaptor();

    void start();

    void stop();

    void set_frame_ready_callback(FrameReadyCallback callback);

    bool try_pop_frame(AVFramePtr &out_frame);

    bool is_running() const { return is_capturing.load(); }

private:
    void init_ctx();

    void init_sws_ctx();

    void capture_loop();

    void receive_frame0(AVPacketPtr obj_packet);

    void receive_frame1(AVPacketPtr obj_packet);

    void notify_frame_ready();

private:
    FrameReadyCallback frame_ready_callback;

    std::mutex callback_mutex;
    std::thread cap_thread;
    std::atomic_bool is_capturing = false;

    std::unique_ptr<DataSafeQueue<AVFramePtr> > queue;
    std::function<void(AVPacketPtr)> decode_func;
    enum AVPixelFormat target_pixel_format = AV_PIX_FMT_BGRA;

    const AVInputFormat *av_input_format = nullptr;
    const AVCodec *av_codec = nullptr;
    AVFormatContextPtr av_format_context = nullptr;
    AVCodecContextPtr av_codec_context = nullptr;
    AVDictionary *options = nullptr;
    AVStreamPtr video_stream = nullptr;
    SwsContextPtr sws_context = nullptr;

    bool change_fmt = false;

    int video_index = -1;


};


#endif //OBSIM_CAPTOR_H
