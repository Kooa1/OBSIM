//
// Created by 66 on 2026/4/26.
//

#include "screencaptor.h"


#include "../utils/av_err2str_cxx.h"

ScreenCaptor::ScreenCaptor() {
    init_ctx();
}

ScreenCaptor::~ScreenCaptor() {
    stop();
}

void ScreenCaptor::start() {
    queue = std::make_unique<DataSafeQueue<AVFramePtr> >(64);

    if (is_capturing.load()) return; // 防止重复启动

    is_capturing.store(true);

    cap_thread = std::thread([this]() {
        capture_loop();
    });
}

void ScreenCaptor::stop() {
    is_capturing.store(false);

    if (cap_thread.joinable()) {
        cap_thread.join();
    }

    if (queue) {
        queue->clean_queue();
    }
}

void ScreenCaptor::set_frame_ready_callback(FrameReadyCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex);
    frame_ready_callback = std::move(callback);
}

bool ScreenCaptor::try_pop_frame(AVFramePtr &out_frame) {
    if (!queue) return false;
    return queue->pop_with_drain(out_frame);
}

void ScreenCaptor::init_ctx() {
    av_input_format = av_find_input_format("gdigrab");
    av_codec_context.reset(avcodec_alloc_context3(nullptr));

    if (!av_input_format || !av_codec_context) {
        av_log(&av_input_format,AV_LOG_ERROR, "input format init fault");
        av_log(av_codec_context.get(), AV_LOG_ERROR, "codec context init fault");
        return;
    }

    auto m_d_m = new DisplayManager();
    DisplayManager::run();

    // return;

    AVFormatContext *fm_ctx = nullptr;
    av_dict_set(&options, "framerate", "30", 0);
    av_dict_set(&options, "video_size", "1920x1080", 0);

    if (const int ret = avformat_open_input(&fm_ctx, "desktop", av_input_format, &options); ret != 0) {
        av_log(nullptr, AV_LOG_ERROR, "format context open failed", av_error_cxx(ret).c_str());
        return;
    }

    av_format_context.reset(fm_ctx, AVFormatContextDeleter());
    av_format_context->probesize = 100000000;
    av_format_context->max_analyze_duration = 5 * AV_TIME_BASE;

    if (const int ret = avformat_find_stream_info(av_format_context.get(), nullptr); ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "no find any stream info", av_error_cxx(ret).c_str());
        return;
    }

    av_dump_format(av_format_context.get(), 0, ":0,0", 0);
    // audio_index = av_find_best_stream(av_format_context.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    video_index = av_find_best_stream(av_format_context.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_index < 0) {
        av_log(nullptr, AV_LOG_ERROR, "not found any video stream : %s\n", av_error_cxx(video_index).c_str());
        return;
    }
    // cout << audio_index << "\n";
    cout << video_index << "\n";

    video_stream.reset(av_format_context->streams[video_index]);

    if (const int ret = avcodec_parameters_to_context(av_codec_context.get(), video_stream->codecpar); ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "parameter to context failed : %s\n", av_error_cxx(ret).c_str());
        return;
    }

    av_codec = avcodec_find_decoder(av_codec_context->codec_id);
    if (!av_codec) {
        av_log(nullptr, AV_LOG_ERROR, "find decodec failed");
        return;
    }

    if (const int ret = avcodec_open2(av_codec_context.get(), av_codec, nullptr); ret != 0) {
        av_log(nullptr, AV_LOG_ERROR, "open2 avcodec failed : %s\n", av_error_cxx(ret).c_str());
        return;
    }

    if (av_codec_context->pix_fmt != target_pixel_format) {
        cout << "not the same\n";
        init_sws_ctx();
        change_fmt = true;
        decode_func = [this](AVPacketPtr video_packet) {
            receive_frame0(std::move(video_packet));
        };
    } else {
        decode_func = [this ](AVPacketPtr video_packet) {
            receive_frame1(std::move(video_packet));
        };
    }

    av_dict_free(&options);
}

void ScreenCaptor::init_sws_ctx() {
    sws_context.reset(sws_getContext(
        av_codec_context->width,
        av_codec_context->height,
        av_codec_context->pix_fmt,
        av_codec_context->width,
        av_codec_context->height,
        target_pixel_format, 0, nullptr, nullptr, nullptr));

    if (!sws_context) {
        av_log(nullptr, AV_LOG_ERROR, "init sws ctx fault\n");
    }
}

void ScreenCaptor::capture_loop() {
    std::cout << "capture_loop STARTED" << std::endl;

    AVPacketPtr av_packet;
    while (is_capturing.load()) {
        av_packet.reset(av_packet_alloc(), AVPacketDeleter());

        if (const int ret = av_read_frame(av_format_context.get(), av_packet.get()); ret == AVERROR_EOF) {
            break;
        } else if (ret == AVERROR(EAGAIN) || ret < 0) {
            continue;
        }

        if (av_packet->stream_index == video_index) {
            decode_func(std::move(av_packet));
        }
    }
}

void ScreenCaptor::receive_frame0(AVPacketPtr obj_packet) {
    auto packet = std::move(obj_packet);
    int ret = avcodec_send_packet(av_codec_context.get(), packet.get());

    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "send video packet failed: %s\n", av_error_cxx(ret).c_str());
        return;
    }

    while (ret >= 0) {
        AVFramePtr ultimate_frame(av_frame_alloc(), AVFrameDeleter());
        if (!ultimate_frame) return;

        ret = avcodec_receive_frame(av_codec_context.get(), ultimate_frame.get());
        if (ret == AVERROR(EAGAIN)) {
            av_log(nullptr, AV_LOG_DEBUG, "need more data : %s\n", av_error_cxx(ret).c_str());
            break;
        }

        if (ret == AVERROR_EOF) {
            av_log(nullptr, AV_LOG_ERROR, "video read file end : %s\n", av_error_cxx(ret).c_str());
            break;
        }

        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Receive frame failed: %s\n", av_error_cxx(ret).c_str());
            continue;
        }

        AVFramePtr dest_frame(av_frame_alloc(), AVFrameDeleter());
        if (!dest_frame) {
            av_log(nullptr, AV_LOG_ERROR, "Failed to allocate dest_frame\n");
            continue;
        }

        dest_frame->format = target_pixel_format;
        dest_frame->width = av_codec_context->width;
        dest_frame->height = av_codec_context->height;

        ret = av_frame_get_buffer(dest_frame.get(), 0);
        if (ret != 0) {
            av_log(nullptr, AV_LOG_ERROR, "Failed to allocate dest buffer\n");
            continue;
        }

        ret = sws_scale(sws_context.get(),
                        ultimate_frame->data,
                        ultimate_frame->linesize,
                        0,
                        av_codec_context->height,
                        dest_frame->data,
                        dest_frame->linesize);

        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "sws_scale failed\n");
            continue;
        }

        if (queue->push(std::move(dest_frame))) {
            notify_frame_ready();
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }
}

void ScreenCaptor::receive_frame1(AVPacketPtr obj_packet) {
    auto packet = std::move(obj_packet);
    int ret = avcodec_send_packet(av_codec_context.get(), packet.get());

    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "send video packet failed: %s\n", av_error_cxx(ret).c_str());
        return;
    }

    while (ret >= 0) {
        AVFramePtr ultimate_frame(av_frame_alloc(), AVFrameDeleter());
        if (!ultimate_frame) return;

        ret = avcodec_receive_frame(av_codec_context.get(), ultimate_frame.get());
        if (ret == AVERROR(EAGAIN)) {
            av_log(nullptr, AV_LOG_DEBUG, "need more data : %s\n", av_error_cxx(ret).c_str());
            break;
        }

        if (ret == AVERROR_EOF) {
            av_log(nullptr, AV_LOG_ERROR, "video read file end : %s\n", av_error_cxx(ret).c_str());
            break;
        }

        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "Receive frame failed: %s\n", av_error_cxx(ret).c_str());
            continue;
        }

        if (queue->push(std::move(ultimate_frame))) {
            notify_frame_ready();
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }
}

void ScreenCaptor::notify_frame_ready() {
    std::lock_guard<std::mutex> lock(callback_mutex);
    if (frame_ready_callback) {
        frame_ready_callback();
    }
}
