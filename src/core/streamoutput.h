#ifndef OBSIM_STREAMOUTPUT_H
#define OBSIM_STREAMOUTPUT_H

#include <atomic>
#include <thread>
#include "../utils/datasafequeue.h"
#include "outputchannel.h"

class StreamOutput : public OutputChannel {
public:
    StreamOutput();
    ~StreamOutput() override;

    bool start(const std::string &rtmp_url,
               AVCodecParameters *video_codecpar,
               AVCodecParameters *audio_codecpar,
               AVRational video_time_base,
               AVRational audio_time_base) override;

    void stop() override;

    void feed_packet(AVPacketPtr pkt) override;

    bool is_running() const override;

private:
    void write_loop();

    std::string m_rtmp_url;
    AVFormatOutputContextPtr m_fmt_ctx;
    int m_video_stream_index = -1;
    int m_audio_stream_index = -1;
    bool m_has_audio = false;

    AVRational m_video_time_base{1, 30};
    AVRational m_audio_time_base{1, 48000};

    std::unique_ptr<DataSafeQueue<AVPacketPtr>> m_packet_queue;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_flush_done{false};
    std::thread m_write_thread;
};

#endif
