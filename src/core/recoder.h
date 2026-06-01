#ifndef OBSIM_RECODER_H
#define OBSIM_RECODER_H

#include <atomic>
#include <thread>
#include <memory>
#include <vector>
#include <deque>
#include <mutex>
#include <algorithm>
#include <future>

#include "../utils/ffmpegfactory.h"
#include "../utils/datasafequeue.h"
#include "../utils/av_err2str_cxx.h"
#include "base/outputchannel.h"

extern "C" {
#include <libavutil/log.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

struct VideoFrame {
    std::vector<uint8_t> data;
    int width = 0;
    int height = 0;
    int stride = 0;
};

class Recoder {
public:
    Recoder();

    virtual ~Recoder();

    void start(const std::string &output_path, int canvas_w, int canvas_h, int fps = 30,
               DataSafeQueue<AVFramePtr> *system_audio_src = nullptr,
               DataSafeQueue<AVFramePtr> *mic_audio_src = nullptr);

    void stop();

    bool is_recording() const { return m_recording.load(); }

    void set_system_volume(float vol) { m_system_volume.store(vol); }
    void set_mic_volume(float vol) { m_mic_volume.store(vol); }
    float get_system_volume() const { return m_system_volume.load(); }
    float get_mic_volume() const { return m_mic_volume.load(); }

    void set_system_muted(bool muted) { m_system_muted.store(muted); }
    void set_mic_muted(bool muted) { m_mic_muted.store(muted); }
    bool is_system_muted() const { return m_system_muted.load(); }
    bool is_mic_muted() const { return m_mic_muted.load(); }

    void feed_frame(const uint8_t *data, int stride, int capture_w, int capture_h);

    void register_output(OutputChannel *channel);
    void unregister_output(OutputChannel *channel);
    size_t output_count() const;

    AVCodecParameters *video_codecpar() const;
    AVCodecParameters *audio_codecpar() const;
    bool has_audio() const { return m_has_audio; }
    AVRational video_time_base() const;
    AVRational audio_time_base() const;

private:
    void encoding_loop();

    bool init_audio_swr(SwrContextPtr &swr, const AVFrame *frame);

    void encode_video_frame(int64_t pts);

    bool encode_audio_frame();

    bool init_codecs();

    void distribute_video_packet();
    void distribute_audio_packet();

    void main_encode_loop();

    void process_system_audio();

    void process_mic_audio();

    void mix_audio_streams();

    void process_video_frame();

    void encode_audio_frames_from_fifo();

    void drain_video_frames();

    void drain_audio_residual();

    void flush_encoders();

    void reset_state();

private:
    static constexpr int AUDIO_SAMPLE_RATE = 48000;
    static constexpr int AUDIO_CHANNELS = 2;
    static constexpr AVSampleFormat AUDIO_FORMAT = AV_SAMPLE_FMT_FLTP;
    static constexpr int AUDIO_FRAME_SAMPLES = 1024;
    static constexpr size_t MAX_FIFO_SIZE = 48000 * 10;

    std::atomic<bool> m_recording{false};
    std::thread m_encode_thread;

    std::shared_ptr<DataSafeQueue<VideoFrame>> m_video_queue;
    DataSafeQueue<AVFramePtr> *m_system_audio_src = nullptr;
    DataSafeQueue<AVFramePtr> *m_mic_audio_src = nullptr;

    int m_canvas_w = 0;
    int m_canvas_h = 0;
    int m_fps = 30;

    AVCodecContextPtr m_video_enc_ctx;
    AVCodecParameters *m_video_codecpar = nullptr;
    AVCodecContextPtr m_audio_enc_ctx;
    AVCodecParameters *m_audio_codecpar = nullptr;
    mutable std::mutex m_channels_mutex;
    std::vector<OutputChannel *> m_output_channels;

    SwsContextPtr m_sws_ctx;
    SwrContextPtr m_sys_swr;
    SwrContextPtr m_mic_swr;
    AVFramePtr m_yuv_frame;
    AVFramePtr m_audio_frame;
    AVPacketPtr m_pkt;

    bool m_has_audio = false;
    int m_audio_frame_size = AUDIO_FRAME_SAMPLES;
    int m_last_capture_w = 0;
    int m_last_capture_h = 0;

    int64_t m_audio_clock = 0;
    int64_t m_video_pts = 0;
    int64_t m_audio_pts = 0;
    int64_t m_audio_frames_received = 0;
    int64_t m_audio_frames_encoded = 0;
    int64_t m_sys_silence_samples = 0;
    int64_t m_mic_silence_samples = 0;

    std::deque<float> m_audio_fifo[2];
    std::deque<float> m_sys_fifo[2];
    std::deque<float> m_mic_fifo[2];

    std::atomic<float> m_system_volume{0.7f};
    std::atomic<float> m_mic_volume{0.7f};
    std::atomic<bool> m_system_muted{false};
    std::atomic<bool> m_mic_muted{false};
};

#endif
