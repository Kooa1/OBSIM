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
    bool init_video_encoder(AVFormatContext *fmt_ctx,
                            AVCodecContext *&video_enc_ctx, AVStream *&video_stream);
    bool init_audio_encoder(AVFormatContext *fmt_ctx,
                            AVCodecContext *&audio_enc_ctx, AVStream *&audio_stream);
    bool open_output_and_allocate_frames(AVFormatContext *fmt_ctx,
                                         AVFrame *&yuv_frame, AVFrame *&audio_frame,
                                         AVPacket *&pkt, AVCodecContext *audio_enc_ctx,
                                         bool has_audio);

    // --- audio processing ---
    void handle_audio_source(SwrContext *&swr, DataSafeQueue<AVFramePtr> *src,
                             std::deque<float> *fifo, int64_t &silence_counter,
                             bool &did_work);
    size_t mix_audio_sources(std::deque<float> *sys_fifo, std::deque<float> *mic_fifo,
                             std::deque<float> *audio_fifo,
                             int64_t &audio_clock, int &frames_received);
    void encode_audio_fifo(AVFormatContext *fmt_ctx, AVPacket *pkt,
                           AVCodecContext *audio_enc_ctx, AVStream *audio_stream,
                           AVFrame *audio_frame, std::deque<float> *audio_fifo,
                           int frame_size, int64_t &audio_pts, int &encoded_count);

    // --- video processing ---
    void handle_video_frame(SwsContext *&sws_ctx, AVFrame *yuv_frame,
                            AVFormatContext *fmt_ctx, AVPacket *pkt,
                            AVCodecContext *video_enc_ctx, AVStream *video_stream,
                            AVCodecContext *audio_enc_ctx,
                            const VideoFrame &video_data, int64_t &video_pts,
                            int64_t audio_clock, bool has_audio,
                            int &last_w, int &last_h);

    // --- drain / flush ---
    void drain_video_queue(SwsContext *&sws_ctx, AVFrame *yuv_frame,
                           AVFormatContext *fmt_ctx, AVPacket *pkt,
                           AVCodecContext *video_enc_ctx, AVStream *video_stream,
                           int64_t &video_pts, int &last_w, int &last_h);
    void flush_swr_and_mix_remaining(SwrContext *sys_swr, SwrContext *mic_swr,
                                     std::deque<float> *sys_fifo,
                                     std::deque<float> *mic_fifo,
                                     std::deque<float> *audio_fifo);
    void drain_audio_fifo_remaining(AVFormatContext *fmt_ctx, AVPacket *pkt,
                                    AVCodecContext *audio_enc_ctx, AVStream *audio_stream,
                                    AVFrame *audio_frame, std::deque<float> *audio_fifo,
                                    int frame_size, int64_t &audio_pts,
                                    int &encoded_count);
    void flush_encoders(AVFormatContext *fmt_ctx, AVPacket *pkt,
                        AVCodecContext *video_enc_ctx, AVStream *video_stream,
                        AVCodecContext *audio_enc_ctx, AVStream *audio_stream);

    // --- low-level helpers ---
    bool init_audio_swr(SwrContext *&swr, const AVFrame *frame, int out_sample_rate);
    void encode_video_frame(AVFormatContext *fmt_ctx, AVPacket *pkt,
                            AVCodecContext *video_enc_ctx, AVStream *video_stream,
                            AVFrame *yuv_frame, int64_t pts);
    bool encode_audio_frame(AVFormatContext *fmt_ctx, AVPacket *pkt,
                            AVCodecContext *audio_enc_ctx, AVStream *audio_stream,
                            AVFrame *audio_frame, int64_t &audio_pts);

    static constexpr int AUDIO_SAMPLE_RATE = 48000;
    static constexpr int AUDIO_CHANNELS = 2;
    static constexpr AVSampleFormat AUDIO_FORMAT = AV_SAMPLE_FMT_FLTP;
    static constexpr int AUDIO_FRAME_SAMPLES = 1024;
    static constexpr size_t MAX_FIFO_SIZE = 48000 * 10;

    std::atomic<bool> m_recording{false};
    std::thread m_encode_thread;
    std::unique_ptr<DataSafeQueue<VideoFrame>> m_video_queue;

    DataSafeQueue<AVFramePtr> *m_system_audio_src = nullptr;
    DataSafeQueue<AVFramePtr> *m_mic_audio_src = nullptr;

    int m_canvas_w = 0;
    int m_canvas_h = 0;
    int m_fps = 30;
    QString m_output_path;
};

#endif
