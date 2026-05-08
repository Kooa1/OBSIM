//
// Created by 66 on 2026/4/26.
//

#ifndef OBSIM_CAPTOR_H
#define OBSIM_CAPTOR_H

#include <iostream>

#include "../utils/ffmpegfactory.h"
#include "../utils/displaymanager.h"
#include "../utils/datasafequeue.h"

extern "C" {
#include "libavutil/avutil.h"
#include "libavutil/pixdesc.h"
}

using std::cout;

class ScreenCaptor {
public:
    explicit ScreenCaptor(std::unique_ptr<DataSafeQueue<AVFramePtr> > &queue);

    ~ScreenCaptor();

    void start_capturing();

private:
    void init_ctx();

    void init_sws_ctx();

    void receive_frame0(AVPacketPtr obj_packet);

    void receive_frame1(AVPacketPtr obj_packet);

private:
    std::unique_ptr<DataSafeQueue<AVFramePtr> > &queue;

    std::function<void(AVPacketPtr)> decode_func;
    enum AVPixelFormat target_pixel_format = AV_PIX_FMT_BGRA;
    bool change_fmt = false;

    const AVInputFormat *av_input_format = nullptr;
    const AVCodec *av_codec = nullptr;
    AVFormatContextPtr av_format_context = nullptr;
    AVCodecContextPtr av_codec_context = nullptr;
    AVDictionary *options = nullptr;
    AVStreamPtr video_stream = nullptr;
    SwsContextPtr sws_context = nullptr;

    int audio_index = -1;
    int video_index = -1;


};


#endif //OBSIM_CAPTOR_H
