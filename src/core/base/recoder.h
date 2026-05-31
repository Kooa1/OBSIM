#ifndef OBSIM_RECODER_H
#define OBSIM_RECODER_H

#include <atomic>
#include <thread>
#include <memory>
#include <vector>
#include <deque>

#include "../../utils/ffmpegfactory.h"
#include "../../utils/datasafequeue.h"
#include "../../utils/av_err2str_cxx.h"

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

/// @brief Represents a raw video frame with pixel data for encoding.
struct VideoFrame {
    std::vector<uint8_t> data;  ///< Raw pixel data (BGRA format).
    int width = 0;              ///< Frame width in pixels.
    int height = 0;             ///< Frame height in pixels.
    int stride = 0;             ///< Stride (bytes per row).
};

/// @brief Encodes video (and optional audio) into an output file.
/// Handles FFmpeg encoding pipeline: color conversion, audio resampling,
/// audio mixing from multiple sources, and output muxing.
class Recoder {
public:
    Recoder();

    virtual ~Recoder();

    /// @brief Starts recording with the given output path and canvas dimensions.
    /// @param output_path Path to the output file.
    /// @param canvas_w Canvas width in pixels.
    /// @param canvas_h Canvas height in pixels.
    /// @param fps Target frame rate.
    /// @param system_audio_src Optional queue for system audio frames.
    /// @param mic_audio_src Optional queue for microphone audio frames.
    virtual void start(const std::string &output_path, int canvas_w, int canvas_h, int fps = 30,
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

    /// @brief Feeds a raw BGRA video frame into the encoding queue.
    void feed_frame(const uint8_t *data, int stride, int capture_w, int capture_h);

protected:
    /// @brief Creates the FFmpeg output format context for the specific output type.
    virtual AVFormatOutputContextPtr create_format_context() = 0;

    /// @brief Opens the output IO context (e.g. file, network).
    virtual bool open_io(AVFormatOutputContextPtr &fmt_ctx) = 0;

private:
    void encoding_loop();

    bool init_audio_swr(SwrContextPtr &swr, const AVFrame *frame);

    void encode_video_frame(int64_t pts);

    bool encode_audio_frame();

    bool init_contexts();

    void main_encode_loop();

    void process_system_audio();

    void process_mic_audio();

    void mix_audio_streams();

    void process_video_frame();

    void encode_audio_frames_from_fifo();

    void drain_video_frames();

    void drain_audio_residual();

    void flush_encoders();

    void write_loop();

    void reset_state();

private:
    static constexpr int AUDIO_SAMPLE_RATE = 48000;          ///< Output audio sample rate.
    static constexpr int AUDIO_CHANNELS = 2;                 ///< Output audio channels (stereo).
    static constexpr AVSampleFormat AUDIO_FORMAT = AV_SAMPLE_FMT_FLTP;  ///< Output audio format.
    static constexpr int AUDIO_FRAME_SAMPLES = 1024;         ///< Samples per audio frame.
    static constexpr size_t MAX_FIFO_SIZE = 48000 * 10;      ///< Max FIFO capacity (10 seconds).

    std::atomic<bool> m_recording{false};                    ///< Whether recording is active.
    std::thread m_encode_thread;                             ///< Encoding thread.
    std::thread m_write_thread;                              ///< Packet write thread.
    std::unique_ptr<DataSafeQueue<VideoFrame> > m_video_queue;  ///< Input queue for video frames.
    DataSafeQueue<AVPacketPtr> m_packet_queue{300};          ///< Packet queue for async write.

    DataSafeQueue<AVFramePtr> *m_system_audio_src = nullptr;  ///< System audio source queue.
    DataSafeQueue<AVFramePtr> *m_mic_audio_src = nullptr;    ///< Microphone audio source queue.

    int m_canvas_w = 0;                                      ///< Canvas width for output video.
    int m_canvas_h = 0;                                      ///< Canvas height for output video.
    int m_fps = 30;                                          ///< Target frame rate.

    AVFormatOutputContextPtr m_fmt_ctx;                      ///< FFmpeg output format context.
    AVCodecContextPtr m_video_enc_ctx;                       ///< Video encoder context.
    AVStream *m_video_stream = nullptr;                      ///< Output video stream.
    AVCodecContextPtr m_audio_enc_ctx;                       ///< Audio encoder context.
    AVStream *m_audio_stream = nullptr;                      ///< Output audio stream.
    SwsContextPtr m_sws_ctx;                                 ///< SWS context for RGB->YUV conversion.
    SwrContextPtr m_sys_swr;                                 ///< SWR context for system audio resampling.
    SwrContextPtr m_mic_swr;                                 ///< SWR context for mic audio resampling.
    AVFramePtr m_yuv_frame;                                  ///< YUV frame buffer for encoding.
    AVFramePtr m_audio_frame;                                ///< Audio frame buffer for encoding.
    AVPacketPtr m_pkt;                                       ///< Reusable packet for encoded output.

    bool m_has_audio = false;                                ///< Whether audio streams are configured.
    int m_audio_frame_size = AUDIO_FRAME_SAMPLES;            ///< Actual audio frame size.
    int m_last_capture_w = 0;                                ///< Last captured frame width.
    int m_last_capture_h = 0;                                ///< Last captured frame height.

    int64_t m_audio_clock = 0;                               ///< Audio sample clock counter.
    int64_t m_video_pts = 0;                                 ///< Video PTS counter.
    int64_t m_audio_pts = 0;                                 ///< Audio PTS counter.
    int64_t m_audio_frames_received = 0;                     ///< Total audio samples received.
    int64_t m_audio_frames_encoded = 0;                      ///< Total audio samples encoded.
    int64_t m_sys_silence_samples = 0;                       ///< Consecutive silence samples from system.
    int64_t m_mic_silence_samples = 0;                       ///< Consecutive silence samples from mic.

    std::deque<float> m_audio_fifo[2];                       ///< Mixed audio FIFO (L/R channels).
    std::deque<float> m_sys_fifo[2];                         ///< System audio FIFO (L/R channels).
    std::deque<float> m_mic_fifo[2];                         ///< Mic audio FIFO (L/R channels).

    std::atomic<float> m_system_volume{0.7f};                ///< System audio volume.
    std::atomic<float> m_mic_volume{0.7f};                   ///< Microphone audio volume.
    std::atomic<bool> m_system_muted{false};                  ///< System audio mute flag.
    std::atomic<bool> m_mic_muted{false};                     ///< Microphone audio mute flag.
};

#endif