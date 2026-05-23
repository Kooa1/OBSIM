#ifndef OBSIM_AUDIOCAPTOR_H
#define OBSIM_AUDIOCAPTOR_H

#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <optional>
#include <mutex>

#include <QDebug>

#include "../utils/ffmpegfactory.h"
#include "../utils/datasafequeue.h"
#include "../utils/av_err2str_cxx.h"

extern "C" {
#include <libavutil/log.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

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

    // 推送帧到录音队列（如果存在）
    void push_to_record_queue(const AVFramePtr &frame);

    // ===== 子类必须实现 =====
    virtual const char *get_input_format_name() const = 0;

    virtual const char *get_device_name() const = 0;

    virtual void setup_options(AVDictionary **opts) {
    }

    // ===== 成员变量（protected 允许子类访问） =====
    bool m_initialized = false;
    std::unique_ptr<DataSafeQueue<AVFramePtr>> queue;
    DataSafeQueue<AVFramePtr> *record_queue = nullptr;
    std::atomic_bool is_capturing{false};
    std::thread cap_thread;
    FrameReadyCallback frame_ready_callback;
    std::mutex callback_mutex;

private:

    std::function<void(AVPacketPtr)> decode_func;

    const AVInputFormat *av_input_format = nullptr;
    const AVCodec *av_decoder = nullptr;
    AVFormatContextPtr av_format_context = nullptr;
    AVCodecContextPtr av_codec_context = nullptr;
    AVDictionary *options = nullptr;
    AVStreamPtr audio_stream = nullptr;
    int audio_index = -1;
};

#endif