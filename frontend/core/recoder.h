#ifndef OBSIM_RECODER_H
#define OBSIM_RECODER_H

#include <atomic>
#include <thread>
#include <memory>
#include <vector>
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

    bool init_audio_swr(SwrContext *&swr, const AVFrame *frame, int out_sample_rate);
    void encode_video_frame(AVFormatContext *fmt_ctx, AVPacket *pkt,
                            AVCodecContext *video_enc_ctx, AVStream *video_stream,
                            AVFrame *yuv_frame, int64_t pts);
    bool encode_audio_frame(AVFormatContext *fmt_ctx, AVPacket *pkt,
                            AVCodecContext *audio_enc_ctx, AVStream *audio_stream,
                            AVFrame *audio_frame, int64_t &audio_pts);

    bool setup_recording(AVFormatContext *&fmt_ctx,
                         AVCodecContext *&video_enc_ctx, AVStream *&video_stream,
                         AVCodecContext *&audio_enc_ctx, AVStream *&audio_stream,
                         bool &has_audio, int &audio_frame_size,
                         AVFrame *&yuv_frame, AVFrame *&audio_frame, AVPacket *&pkt);
    void main_encode_loop(AVFormatContext *fmt_ctx, AVPacket *pkt,
                          AVCodecContext *video_enc_ctx, AVStream *video_stream,
                          AVCodecContext *audio_enc_ctx, AVStream *audio_stream,
                          SwsContext *&sws_ctx, SwrContext *sys_swr, SwrContext *mic_swr,
                          AVFrame *yuv_frame, AVFrame *audio_frame,
                          bool has_audio, int audio_frame_size,
                          std::deque<float> *audio_fifo,
                          std::deque<float> *sys_fifo,
                          std::deque<float> *mic_fifo,
                          int64_t &audio_clock, int64_t &video_pts, int64_t &audio_pts,
                          int64_t &audio_frames_received, int64_t &audio_frames_encoded,
                          int64_t &sys_silence_samples, int64_t &mic_silence_samples,
                          int &last_capture_w, int &last_capture_h);

    void process_audio_source(DataSafeQueue<AVFramePtr> *src, SwrContext *swr,
                              int64_t &silence_counter, std::deque<float> *fifo,
                              bool &did_work);
    void mix_audio(std::deque<float> *sys_fifo, std::deque<float> *mic_fifo,
                   std::deque<float> *audio_fifo, int64_t &audio_frames_received,
                   int64_t &audio_clock, bool &did_work);
    void process_video_frame(SwsContext *&sws_ctx, AVFrame *yuv_frame,
                             AVFormatContext *fmt_ctx, AVPacket *pkt,
                             AVCodecContext *video_enc_ctx, AVStream *video_stream,
                             int64_t &video_pts, bool has_audio,
                             AVCodecContext *audio_enc_ctx, int64_t audio_clock,
                             int &last_capture_w, int &last_capture_h, bool &did_work);
    void encode_audio_frames(AVFrame *audio_frame, std::deque<float> *audio_fifo,
                             int audio_frame_size, AVFormatContext *fmt_ctx, AVPacket *pkt,
                             AVCodecContext *audio_enc_ctx, AVStream *audio_stream,
                             int64_t &audio_pts, int64_t &audio_frames_encoded, bool &did_work);

    void drain_and_finalize(AVFormatContext *fmt_ctx, AVPacket *pkt,
                            AVCodecContext *video_enc_ctx, AVStream *video_stream,
                            AVCodecContext *audio_enc_ctx, AVStream *audio_stream,
                            SwsContext *&sws_ctx, SwrContext *sys_swr, SwrContext *mic_swr,
                            AVFrame *yuv_frame, AVFrame *audio_frame,
                            bool has_audio, int audio_frame_size,
                            std::deque<float> *audio_fifo,
                            std::deque<float> *sys_fifo,
                            std::deque<float> *mic_fifo,
                            int64_t &video_pts, int64_t &audio_pts,
                            int64_t &audio_frames_encoded,
                            int &last_capture_w, int &last_capture_h);
    void drain_video_frames(SwsContext *&sws_ctx, AVFrame *yuv_frame,
                            AVFormatContext *fmt_ctx, AVPacket *pkt,
                            AVCodecContext *video_enc_ctx, AVStream *video_stream,
                            int64_t &video_pts, int &last_capture_w, int &last_capture_h);
    void flush_resamplers_and_mix_residual(SwrContext *sys_swr, SwrContext *mic_swr,
                                           std::deque<float> *sys_fifo, std::deque<float> *mic_fifo,
                                           std::deque<float> *audio_fifo, bool has_audio);
    void drain_audio_fifo_residual(AVFrame *audio_frame, std::deque<float> *audio_fifo,
                                   int audio_frame_size, AVFormatContext *fmt_ctx,
                                   AVPacket *pkt, AVCodecContext *audio_enc_ctx,
                                   AVStream *audio_stream, int64_t &audio_pts,
                                   int64_t &audio_frames_encoded, bool has_audio);
    void flush_encoders_and_trailer(AVFormatContext *fmt_ctx, AVPacket *pkt,
                                    AVCodecContext *video_enc_ctx, AVStream *video_stream,
                                    AVCodecContext *audio_enc_ctx, AVStream *audio_stream,
                                    bool has_audio);
    void cleanup_resources(AVFormatContext *&fmt_ctx, AVPacket *&pkt,
                           AVFrame *&yuv_frame, AVFrame *&audio_frame,
                           SwsContext *&sws_ctx, SwrContext *&sys_swr, SwrContext *&mic_swr,
                           AVCodecContext *&video_enc_ctx, AVCodecContext *&audio_enc_ctx);

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
