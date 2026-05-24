#ifndef OBSIM_RECODER_H
#define OBSIM_RECODER_H

#include <atomic>
#include <thread>
#include <memory>
#include <vector>
#include <chrono>
#include <queue>
#include <deque>

#include <QString>

#include "../utils/ffmpegfactory.h"
#include "../utils/datasafequeue.h"

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

    ~Recoder();

    void start(const QString &output_path, int canvas_w, int canvas_h, int fps = 30,
               DataSafeQueue<AVFramePtr> *system_audio_src = nullptr,
               DataSafeQueue<AVFramePtr> *mic_audio_src = nullptr);

    void stop();

    bool is_recording() const { return m_recording.load(); }

    void feed_frame(const uint8_t *data, int stride, int capture_w, int capture_h);

private:
    void encoding_loop();

    // --- encoder setup ---
    bool init_video_encoder();

    bool init_audio_encoder();

    bool open_output_and_allocate_frames();

    // --- audio processing ---
    void handle_audio_source(DataSafeQueue<AVFramePtr> *src,
                             SwrContextPtr &swr,
                             std::deque<float> *fifo,
                             int64_t &silence_counter,
                             bool &did_work);

    size_t mix_audio_sources();

    void encode_audio_fifo();

    // --- video processing ---
    void handle_video_frame(const VideoFrame &video_data);

    // --- drain / flush ---
    void drain_video_queue();

    void flush_swr_and_mix_remaining();

    void drain_audio_fifo_remaining();

    void flush_encoders();

    // --- low-level helpers ---
    bool init_audio_swr(SwrContextPtr &swr, const AVFrame *frame);

    void encode_video_frame(int64_t pts);

    bool encode_audio_frame(int64_t audio_pts);

    void clear_ctx();

private:

    static constexpr int AUDIO_SAMPLE_RATE = 48000;
    static constexpr int AUDIO_CHANNELS = 2;
    static constexpr AVSampleFormat AUDIO_FORMAT = AV_SAMPLE_FMT_FLTP;
    static constexpr int AUDIO_FRAME_SAMPLES = 1024;
    static constexpr size_t MAX_FIFO_SIZE = 48000 * 10;

    std::atomic<bool> m_recording{false};
    std::thread m_encode_thread;
    std::unique_ptr<DataSafeQueue<VideoFrame> > m_video_queue;

    DataSafeQueue<AVFramePtr> *m_system_audio_src = nullptr;
    DataSafeQueue<AVFramePtr> *m_mic_audio_src = nullptr;

    int m_canvas_w = 0;
    int m_canvas_h = 0;
    int m_fps = 30;
    QString m_output_path;

    // FFmpeg smart-pointer resources
    AVFormatMuxPtr m_fmt_ctx;
    AVCodecContextPtr m_video_enc_ctx;
    AVCodecContextPtr m_audio_enc_ctx;
    SwsContextPtr m_sws_ctx;
    SwrContextPtr m_sys_swr;
    SwrContextPtr m_mic_swr;
    AVFramePtr m_yuv_frame;
    AVFramePtr m_audio_frame;
    AVPacketPtr m_pkt;

    // Raw AVStream pointers (owned by AVFormatContext)
    AVStream *m_video_stream = nullptr;
    AVStream *m_audio_stream = nullptr;

    // Encoding state
    std::deque<float> m_audio_fifo[2];
    std::deque<float> m_sys_fifo[2];
    std::deque<float> m_mic_fifo[2];
    int64_t m_audio_clock = 0;
    int64_t m_video_pts = 0;
    int64_t m_audio_pts = 0;
    int m_audio_frames_received = 0;
    int m_audio_frames_encoded = 0;
    int64_t m_sys_silence_samples = 0;
    int64_t m_mic_silence_samples = 0;
    int m_last_capture_w = 0;
    int m_last_capture_h = 0;
    int m_audio_frame_size = 0;
    bool m_has_audio = false;
};

#endif
