#ifndef OBSIM_FILEOUTPUT_H
#define OBSIM_FILEOUTPUT_H

#include <atomic>
#include <thread>
#include "../utils/datasafequeue.h"
#include "outputchannel.h"

class FileOutput : public OutputChannel {
public:
    FileOutput();
    ~FileOutput() override;

    bool start(const std::string &output_path,
               AVCodecParameters *video_codecpar,
               AVCodecParameters *audio_codecpar,
               AVRational video_time_base,
               AVRational audio_time_base) override;

    void stop() override;

    void feed_packet(AVPacketPtr pkt) override;

    bool is_running() const override;

private:
    void write_loop();

    std::string m_output_path;
    AVFormatOutputContextPtr m_fmt_ctx;
    int m_video_stream_index = -1;
    int m_audio_stream_index = -1;
    bool m_has_audio = false;

    std::unique_ptr<DataSafeQueue<AVPacketPtr>> m_packet_queue;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_flush_done{false};
    std::thread m_write_thread;
};

#endif
