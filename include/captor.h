//
// Created by 66 on 2025/12/18.
//

#ifndef MONITORSERVER_CAPTOR_H
#define MONITORSERVER_CAPTOR_H

extern "C" {
#include <libavutil/log.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include <iostream>
#include <fstream>

#include "ffmpegfactory.h"
#include "av_err2str_cxx.h"

using std::cout;

class Captor {
public:
    Captor();

    ~Captor() = default;

private:
    void init_context();

    void decode_video(AVPacketPtr av_packet);

private:
    AVFormatContextPtr av_format_context = nullptr;
    AVCodecContextPtr av_codec_context = nullptr;
    AVStreamPtr video_stream = nullptr;
    SwsContextPtr sws_context = nullptr;
    const AVInputFormat *av_input_format = nullptr;
    const AVCodec *av_coder = nullptr;
    int video_index = 0;

    std::ofstream out_target;
    std::string device_name;;

#ifdef _WIN32
    const std::string path = "D:/Tools/src/test.yuv";
#elif __APPLE__
    const std::string path = "/Users/wuwenze/Desktop/photo/p/test.yuv";
#endif

};


#endif //MONITORSERVER_CAPTOR_H
