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

class Captor {
public:
    explicit Captor();

    ~Captor();

private:
    bool init_ctx();


private:
    const AVInputFormat *av_input_format = nullptr;
    AVFormatContextPtr av_format_context = nullptr;
    AVCodecContextPtr av_codec_context = nullptr;
    AVDictionary *options = nullptr;
};


#endif //OBSIM_CAPTOR_H
