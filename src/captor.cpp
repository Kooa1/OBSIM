//
// Created by 66 on 2025/12/18.
//

#include "../include/captor.h"

Captor::Captor() {
    av_log_set_level(AV_LOG_INFO);
    init_context();
}

void Captor::init_context() {
#ifdef _WIN32
    av_input_format = av_find_input_format("dshow");
    device_name = "video=USB Camera";
#elif __APPLE__
    av_input_format = av_find_input_format("avfoundation");
    device_name = "0";
#endif

    if (!av_input_format) {
        av_log(nullptr, AV_LOG_ERROR, "find devices failed\n");
        return;
    }

    av_format_context.reset(avformat_alloc_context(), AVFormatContextDeleter());
    AVDictionary *options = nullptr;
    av_dict_set(&options, "video_size", "1920x1080", 0);
    av_dict_set(&options, "framerate", "30", 0);

    auto ctx = av_format_context.get();
    if (const int ret = avformat_open_input(&ctx, device_name.c_str(), av_input_format, &options); ret != 0) {
        av_log(nullptr, AV_LOG_ERROR, "open input format failed: %s\n", av_error_cxx(ret).c_str());
        return;
    }

    if (const int ret = avformat_find_stream_info(av_format_context.get(), nullptr); ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "found input format failed: %s\n", av_error_cxx(ret).c_str());
        return;
    }

    if (const int ret = av_find_best_stream(av_format_context.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0); ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "have not best stream: %s\n", av_error_cxx(ret).c_str());
        return;
    } else {
        video_index = ret;
    }

    av_codec_context.reset(avcodec_alloc_context3(nullptr));

    if (const int ret = avcodec_parameters_to_context(av_codec_context.get(),
                                                      av_format_context->streams[video_index]->codecpar); ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "copy parameters failed: %s\n", av_error_cxx(ret).c_str());
        return;
    }

    av_coder = avcodec_find_decoder(av_codec_context->codec_id);
    if (!av_coder) {
        av_log(nullptr, AV_LOG_ERROR, "find decoder failed\n");
        return;
    }

    if (const int ret = avcodec_open2(av_codec_context.get(), av_coder, nullptr); ret != 0) {
        av_log(nullptr, AV_LOG_ERROR, "open decoder failed: %s\n", av_error_cxx(ret).c_str());
        return;
    }

#ifdef _WIN32
    sws_context.reset(sws_getContext(av_codec_context->width,
                                     av_codec_context->height,
                                     av_codec_context->pix_fmt,
                                     av_codec_context->width,
                                     av_codec_context->height,
                                     AV_PIX_FMT_YUV420P,
                                     SWS_FAST_BILINEAR,
                                     nullptr,
                                     nullptr,
                                     nullptr));
#endif

    out_target.open(path, std::ios::binary);
    if (!out_target) {
        std::cerr << "error, target failed\n";
    }

    if (!out_target.is_open()) {
        std::cerr << "error, open file failed\n";
    }

    AVPacketPtr av_packet;
    while (true) {
        av_packet.reset(av_packet_alloc(), AVPacketDeleter());

        if (const int ret = av_read_frame(av_format_context.get(), av_packet.get()); ret == AVERROR_EOF) {
            break;
        } else if (ret == AVERROR(EAGAIN) || ret < 0) {
            continue;
        }

        if (av_packet->stream_index == video_index) {
            decode_video(std::move(av_packet));
        }
    }

    decode_video(nullptr);
}

void Captor::decode_video(AVPacketPtr av_packet) {
    const auto packet = std::move(av_packet);
    if (const int ret = avcodec_send_packet(av_codec_context.get(), packet.get()); ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "send frame failed: %s\n", av_error_cxx(ret).c_str());
        return;
    }

    while (true) {
        AVFramePtr frame;
        frame.reset(av_frame_alloc(), AVFrameDeleter());
        if (!frame) return;

        int ret = avcodec_receive_frame(av_codec_context.get(), frame.get());
        if (ret == AVERROR(EAGAIN)) {
            av_log(nullptr, AV_LOG_DEBUG, "need more data : %s\n", av_error_cxx(ret).c_str());
            break;
        }

        if (ret == AVERROR_EOF) {
            av_log(nullptr, AV_LOG_ERROR, "video read file end : %s\n", av_error_cxx(ret).c_str());
            break;
        }

        if (ret == AVERROR_INVALIDDATA) {
            continue;
        }

        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "receive video frame failed: %s\n", av_error_cxx(ret).c_str());
            continue;
        }

        // cout << "Frame received: width=" << frame->width
        // << ", height=" << frame->height
        // << ", format=" << frame->format
        // << ", pts=" << frame->pts << '\n';

        auto ctx_time_base = av_format_context->streams[video_index]->time_base;
        cout << "frame pts: " << frame->pts * av_q2d(ctx_time_base) << '\n';

#ifdef _WIN32
        sws_scale(sws_context.get(),
                  frame->data,
                  frame->linesize,
                  0,
                  av_codec_context->height,
                  frame->data,
                  frame->linesize
        );

        // Y 分量（完整分辨率）
        for (int i = 0; i < av_codec_context->height; i++) {
            out_target.write(reinterpret_cast<const char *>(frame->data[0] + i * frame->linesize[0]),
                             av_codec_context->width);
        }

        // U 分量（1/4 分辨率，YUV420格式）
        for (int i = 0; i < av_codec_context->height / 2; i++) {
            out_target.write(reinterpret_cast<const char *>(frame->data[1] + i * frame->linesize[1]),
                             av_codec_context->width / 2);
        }

        // V 分量（1/4 分辨率，YUV420格式）
        for (int i = 0; i < av_codec_context->height / 2; i++) {
            out_target.write(reinterpret_cast<const char *>(frame->data[2] + i * frame->linesize[2]),
                             av_codec_context->width / 2);
        }

#elif __APPLE__
        out_target.write(reinterpret_cast<const char *>(frame->data[0]),
                         av_codec_context->width * av_codec_context->height * 2);
#endif
    }
}
