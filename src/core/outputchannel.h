#ifndef OBSIM_OUTPUTCHANNEL_H
#define OBSIM_OUTPUTCHANNEL_H

#include <memory>
#include <string>
#include "../utils/ffmpegfactory.h"

class OutputChannel {
public:
    virtual ~OutputChannel() = default;

    virtual bool start(const std::string &output_path,
                       AVCodecParameters *video_codecpar,
                       AVCodecParameters *audio_codecpar,
                       AVRational video_time_base,
                       AVRational audio_time_base) = 0;

    virtual void stop() = 0;

    virtual void feed_packet(AVPacketPtr pkt) = 0;

    virtual bool is_running() const = 0;
};

#endif
