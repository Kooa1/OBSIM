//
// Created by 66 on 2026/4/26.
//

#ifndef OBSIM_CAPTOR_H
#define OBSIM_CAPTOR_H

#include <iostream>

#include "Utils/ffmpegfactory.h"
#include "../Utils/displaymanager.h"

extern "C" {
#include "libavutil/avutil.h"
}

using std::cout;

class DisplayCaptor {
public:
    explicit DisplayCaptor();

    ~DisplayCaptor();

    void start_capturing();

private:
    void init_ctx();

    void init_dest_frame();

    void init_sws_ctx();

    void receive_frame(AVPacketPtr obj_packet);

private:
    const AVInputFormat *av_input_format = nullptr;
    const AVCodec *av_codec = nullptr;

    AVFormatContextPtr av_format_context = nullptr;
    AVCodecContextPtr av_codec_context = nullptr;
    AVDictionary *options = nullptr;
    AVStreamPtr video_stream = nullptr;
    AVFramePtr dest_frame = nullptr;
    SwsContextPtr sws_context = nullptr;

    int audio_index = -1;
    int video_index = -1;
    enum AVPixelFormat dest_fmt = AV_PIX_FMT_YUV420P;
};


#endif //OBSIM_CAPTOR_H
