//
// Created by 66 on 2026/5/11.
//

#include "videocaptor.h"

VideoCaptor::~VideoCaptor() {
    stop();
}

void VideoCaptor::start() {
    if (is_capturing.load()) return;
    queue = std::make_unique<DataSafeQueue<AVFramePtr>>(64);
    is_capturing.store(true);
    cap_thread = std::thread([this]() {
        init_ctx();
        if (av_format_context && decode_func) {
            capture_loop();
        }
    });
}

void VideoCaptor::stop() {
    is_capturing.store(false);
    if (cap_thread.joinable()) {
        cap_thread.join();
    }
    if (queue) {
        queue->clean_queue();
    }
}

void VideoCaptor::pause() {
    is_paused_.store(true);
    if (queue) {
        queue->clean_queue();
    }
}

void VideoCaptor::resume() {
    is_paused_.store(false);
}

void VideoCaptor::set_frame_ready_callback(FrameReadyCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex);
    frame_ready_callback = std::move(callback);
}

std::optional<AVFramePtr> VideoCaptor::try_pop_frame() {
    if (!queue) return std::nullopt;
    return queue->try_pop_drain();
}

void VideoCaptor::apply_config(const CaptorConfig &config) {
    m_config = config;
}

// ========== FFmpeg 初始化（通用流程） ==========
void VideoCaptor::init_ctx() {
    av_input_format = av_find_input_format(get_input_format_name());
    if (!av_input_format) {
        av_log(nullptr, AV_LOG_ERROR, "find input format failed: %s\n", get_input_format_name());
        return;
    }

    av_codec_context.reset(avcodec_alloc_context3(nullptr));
    if (!av_codec_context) {
        av_log(nullptr, AV_LOG_ERROR, "codec context init fault\n");
        return;
    }

    setup_options(&options);

    AVFormatContext* fm_ctx = nullptr;
    if (int ret = avformat_open_input(&fm_ctx, get_device_name(), av_input_format, &options); ret != 0) {
        av_log(nullptr, AV_LOG_ERROR, "open input format failed: %s\n", av_error_cxx(ret).c_str());
        av_dict_free(&options);
        return;
    }

    av_format_context.reset(fm_ctx, AVFormatContextDeleter());
    av_format_context->probesize = 100000000;
    av_format_context->max_analyze_duration = 5 * AV_TIME_BASE;

    if (int ret = avformat_find_stream_info(av_format_context.get(), nullptr); ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "find stream info failed: %s\n", av_error_cxx(ret).c_str());
        return;
    }

    video_index = av_find_best_stream(av_format_context.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_index < 0) {
        av_log(nullptr, AV_LOG_ERROR, "not found video stream: %s\n", av_error_cxx(video_index).c_str());
        return;
    }

    video_stream.reset(av_format_context->streams[video_index]);

    if (int ret = avcodec_parameters_to_context(av_codec_context.get(), video_stream->codecpar); ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "parameters to context failed: %s\n", av_error_cxx(ret).c_str());
        return;
    }

    av_decoder = avcodec_find_decoder(av_codec_context->codec_id);
    if (!av_decoder) {
        av_log(nullptr, AV_LOG_ERROR, "find decoder failed\n");
        return;
    }

    if (int ret = avcodec_open2(av_codec_context.get(), av_decoder, nullptr); ret != 0) {
        av_log(nullptr, AV_LOG_ERROR, "open2 avcodec failed: %s\n", av_error_cxx(ret).c_str());
        return;
    }

    if (av_codec_context->pix_fmt != target_pixel_format) {
        std::cout << "Pixel format is not BGRA, need sws_scale\n";
        init_sws_ctx();
        change_fmt = true;
        decode_func = [this](AVPacketPtr packet) {
            receive_frame0(std::move(packet));
        };
    } else {
        decode_func = [this](AVPacketPtr packet) {
            receive_frame1(std::move(packet));
        };
    }

    av_dict_free(&options);
}

void VideoCaptor::init_sws_ctx() {
    sws_context.reset(sws_getContext(
        av_codec_context->width,
        av_codec_context->height,
        av_codec_context->pix_fmt,
        av_codec_context->width,
        av_codec_context->height,
        target_pixel_format,
        SWS_FAST_BILINEAR,
        nullptr, nullptr, nullptr));
    if (!sws_context) {
        av_log(nullptr, AV_LOG_ERROR, "init sws ctx fault\n");
    }
}

// ========== 采集循环 ==========
void VideoCaptor::capture_loop() {
    while (is_capturing.load()) {
        if (is_paused_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        AVPacketPtr av_packet(av_packet_alloc(), AVPacketDeleter());
        int ret = av_read_frame(av_format_context.get(), av_packet.get());
        if (ret == AVERROR_EOF) break;
        else if (ret == AVERROR(EAGAIN) || ret < 0) continue;

        if (av_packet->stream_index == video_index) {
            decode_func(std::move(av_packet));
        }
    }
}

// ========== 需要格式转换的接收函数 ==========
void VideoCaptor::receive_frame0(AVPacketPtr obj_packet) {
    auto packet = std::move(obj_packet);
    int ret = 0;
    // 处理 EAGAIN：内部队列满时短暂休眠再重试
    while ((ret = avcodec_send_packet(av_codec_context.get(), packet.get())) == AVERROR(EAGAIN)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "send video packet failed: %s\n", av_error_cxx(ret).c_str());
        return;
    }

    while (ret >= 0) {
        AVFramePtr ultimate_frame(av_frame_alloc(), AVFrameDeleter());
        if (!ultimate_frame) return;
        ret = avcodec_receive_frame(av_codec_context.get(), ultimate_frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) continue;

        AVFramePtr dest_frame(av_frame_alloc(), AVFrameDeleter());
        if (!dest_frame) continue;
        dest_frame->format = target_pixel_format;
        dest_frame->width  = av_codec_context->width;
        dest_frame->height = av_codec_context->height;
        ret = av_frame_get_buffer(dest_frame.get(), 0);
        if (ret != 0) continue;

        ret = sws_scale(sws_context.get(),
                        ultimate_frame->data, ultimate_frame->linesize,
                        0, av_codec_context->height,
                        dest_frame->data, dest_frame->linesize);
        if (ret < 0) continue;

        queue->push_no_wait(std::move(dest_frame));
        notify_frame_ready();
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
}

// ========== 无需格式转换的接收函数 ==========
void VideoCaptor::receive_frame1(AVPacketPtr obj_packet) {
    auto packet = std::move(obj_packet);
    int ret = 0;
    while ((ret = avcodec_send_packet(av_codec_context.get(), packet.get())) == AVERROR(EAGAIN)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "send video packet failed: %s\n", av_error_cxx(ret).c_str());
        return;
    }

    while (ret >= 0) {
        AVFramePtr ultimate_frame(av_frame_alloc(), AVFrameDeleter());
        if (!ultimate_frame) return;
        ret = avcodec_receive_frame(av_codec_context.get(), ultimate_frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) continue;

        queue->push_no_wait(std::move(ultimate_frame));
        notify_frame_ready();
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
}

void VideoCaptor::notify_frame_ready() {
    std::lock_guard<std::mutex> lock(callback_mutex);
    if (frame_ready_callback) {
        frame_ready_callback();
    }
}