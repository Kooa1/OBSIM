#ifndef ZPLAYER_FFMPEGFACTORY_H
#define ZPLAYER_FFMPEGFACTORY_H

#include <memory>

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
};

/// @brief Custom deleter for AVFormatContext (input)
struct AVFormatContextDeleter {
    void operator()(AVFormatContext *context) const {
        if (context) {
            avformat_close_input(&context);
        }
    }
};

/// @brief Custom deleter for AVCodecContext
struct AVCodecContextDeleter {
    void operator()(AVCodecContext *context) const {
        if (context) {
            avcodec_free_context(&context);
        }
    }
};

/// @brief Custom deleter for SwsContext
struct SwsContextDeleter {
    void operator()(struct SwsContext *context) const {
        if (context) {
            sws_freeContext(context);
        }
    }
};

/// @brief Custom deleter for AVPacket
struct AVPacketDeleter {
    void operator()(AVPacket *packet) const {
        if (packet) {
            av_packet_free(&packet);
        }
    }
};

/// @brief Custom deleter for AVStream (no-op)
struct AVStreamDeleter {
    void operator()(AVStream *stream) const {
        if (stream) {
        }
    }
};

/// @brief Custom deleter for AVFrame
struct AVFrameDeleter {
    void operator()(AVFrame *frame) const {
        if (frame) {
            av_frame_free(&frame);
        }
    }
};

/// @brief Custom deleter for AVFormatContext (output)
struct AVFormatOutputContextDeleter {
    void operator()(AVFormatContext *context) const {
        if (context) {
            if (context->pb) avio_close(context->pb);
            avformat_free_context(context);
        }
    }
};

/// @brief Custom deleter for SwrContext
struct SwrContextDeleter {
    void operator()(SwrContext *context) const {
        if (context) {
            swr_free(&context);
        }
    }
};


using AVFormatContextPtr = std::shared_ptr<AVFormatContext>; ///< Shared input format context
using AVFormatOutputContextPtr = std::unique_ptr<AVFormatContext, AVFormatOutputContextDeleter>; ///< Unique output format context
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>; ///< Unique codec context
using SwsContextPtr = std::unique_ptr<struct SwsContext, SwsContextDeleter>; ///< Unique swscale context
using AVPacketPtr = std::shared_ptr<AVPacket>; ///< Shared packet
using AVFramePtr = std::shared_ptr<AVFrame>; ///< Shared frame
using AVStreamPtr = std::unique_ptr<AVStream, AVStreamDeleter>; ///< Unique stream
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>; ///< Unique swresample context

/// @brief Create an AVFormatOutputContext for the given filename
inline AVFormatOutputContextPtr make_output_context(const char *filename, const char *fmt_name = nullptr) {
    AVFormatContext *ctx = nullptr;
    avformat_alloc_output_context2(&ctx, nullptr, fmt_name, filename);
    return AVFormatOutputContextPtr(ctx);
}

/// @brief Get an AVStream by index from an AVFormatContext
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
