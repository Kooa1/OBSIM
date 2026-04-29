//
// Created by 66 on 2025/8/24.
//

#ifndef ZPLAYER_FFMPEGFACTORY_H
#define ZPLAYER_FFMPEGFACTORY_H

#include <memory>

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
};

struct AVFormatContextDeleter {
    void operator()(AVFormatContext *context) const {
        if (context) {
            avformat_close_input(&context);
        }
    }
};

struct AVCodecContextDeleter {
    void operator()(AVCodecContext *context) const {
        if (context) {
            avcodec_free_context(&context);
        }
    }
};

struct SwsContextDeleter {
    void operator()(struct SwsContext *context) const {
        if (context) {
            sws_freeContext(context);
        }
    }
};

struct AVPacketDeleter {
    void operator()(AVPacket *packet) const {
        if (packet) {
            av_packet_free(&packet);
        }
    }
};

struct AVStreamDeleter {
    void operator()(AVStream *stream) const {
        if (stream) {
        }
    }
};

struct AVFrameDeleter {
    void operator()(AVFrame *frame) const {
        if (frame) {
            av_frame_free(&frame);
        }
    }
};

using AVFormatContextPtr = std::shared_ptr<AVFormatContext>;
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using SwsContextPtr = std::unique_ptr<struct SwsContext, SwsContextDeleter>;
using AVPacketPtr = std::shared_ptr<AVPacket>;
using AVFramePtr = std::shared_ptr<AVFrame>;
using AVStreamPtr = std::unique_ptr<AVStream, AVStreamDeleter>;

inline AVStreamPtr get_stream_by_index(AVFormatContextPtr *avFormatContext, int stream_index) {
    if (!avFormatContext || stream_index < 0 ||
        stream_index >= static_cast<int>(avFormatContext->get()->nb_streams)) {
        return nullptr;
    }

    AVStream *raw_stream = avFormatContext->get()->streams[stream_index];

    if (!raw_stream) {
        return nullptr;
    }

    return AVStreamPtr(raw_stream, AVStreamDeleter());
}

#endif //ZPLAYER_FFMPEGFACTORY_H
