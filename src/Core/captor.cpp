//
// Created by 66 on 2026/4/26.
//

#include "Core/captor.h"

#include "Utils/av_err2str_cxx.h"

Captor::Captor() {
    init_ctx();
}

Captor::~Captor() = default;

bool Captor::init_ctx() {
    av_input_format = av_find_input_format("gdigrab");
    av_codec_context.reset(avcodec_alloc_context3(nullptr));

    if (!av_input_format || !av_codec_context) {
        av_log(&av_input_format,AV_LOG_ERROR, "input format init fault");
        av_log(av_codec_context.get(), AV_LOG_ERROR, "codec context init fault");
        return false;
    }

    AVFormatContext *fm_ctx = nullptr;
    av_dict_set(&options, "framerate", "30", 0);
    av_dict_set(&options, "video_size", "1920x1080", 0);
    av_dict_set(&options, "offset_x", "0", 0);
    av_dict_set(&options, "offset_y", "0", 0);

    if (const int ret = avformat_open_input(&fm_ctx, "desktop", av_input_format, &options); ret != 0) {
        av_log(nullptr, AV_LOG_ERROR, "format context open failed", av_error_cxx(ret).c_str());
        return false;
    }

    av_format_context.reset(fm_ctx, AVFormatContextDeleter());

    const auto display_manager = new DisplayManager();
    display_manager->run();


    return true;
}
