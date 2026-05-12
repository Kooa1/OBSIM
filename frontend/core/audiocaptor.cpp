#include "audiocaptor.h"
#include <iostream>
#include <chrono>

AudioCaptor::~AudioCaptor() {
    stop();
}

void AudioCaptor::start() {
    if (is_capturing.load()) return;
    if (!m_initialized) return;

    queue = std::make_unique<DataSafeQueue<AVFramePtr> >(64);
    is_capturing.store(true);
    cap_thread = std::thread([this]() { capture_loop(); });
}

void AudioCaptor::stop() {
    is_capturing.store(false);
    if (cap_thread.joinable()) {
        cap_thread.join();
    }
    if (queue) {
        queue->clean_queue();
    }
}

void AudioCaptor::set_frame_ready_callback(FrameReadyCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex);
    frame_ready_callback = std::move(callback);
}

std::optional<AVFramePtr> AudioCaptor::try_pop_frame() {
    if (!queue || !m_initialized) return std::nullopt;
    return queue->try_pop_drain();
}

void AudioCaptor::init_ctx() {
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

    AVFormatContext *fm_ctx = nullptr;
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

    audio_index = av_find_best_stream(av_format_context.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_index < 0) {
        av_log(nullptr, AV_LOG_ERROR, "not found audio stream: %s\n", av_error_cxx(audio_index).c_str());
        return;
    }

    audio_stream.reset(av_format_context->streams[audio_index]);

    if (int ret = avcodec_parameters_to_context(av_codec_context.get(), audio_stream->codecpar); ret < 0) {
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

    // AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    // AVChannelLayout in_ch_layout = av_codec_context->ch_layout;
    //
    // SwrContext *raw_swr = nullptr;
    // if (swr_alloc_set_opts2(&raw_swr,
    //                         &out_ch_layout,
    //                         target_sample_format,
    //                         target_sample_rate,
    //                         &in_ch_layout,
    //                         av_codec_context->sample_fmt,
    //                         av_codec_context->sample_rate,
    //                         0, nullptr) < 0) {
    //     av_log(nullptr, AV_LOG_ERROR, "swr_alloc_set_opts2 failed\n");
    //     return;
    // }
    // swr_context.reset(raw_swr);
    //
    // if (swr_init(swr_context.get()) < 0) {
    //     av_log(nullptr, AV_LOG_ERROR, "swr_init failed\n");
    //     return;
    // }

    decode_func = [this](AVPacketPtr packet) {
        receive_frame(std::move(packet));
    };

    av_dict_free(&options);
    m_initialized = true;
}

void AudioCaptor::capture_loop() {
    if (!av_format_context) {
        qDebug() << "AudioCaptor: av_format_context is null, cannot start loop";
        return;
    }

    qDebug() << "AudioCaptor: capture_loop started, audio_index =" << audio_index;

    int packet_count = 0;
    int audio_packet_count = 0;

    while (is_capturing.load()) {
        AVPacketPtr av_packet(av_packet_alloc(), AVPacketDeleter());
        int ret = av_read_frame(av_format_context.get(), av_packet.get());
        if (ret == AVERROR_EOF) {
            qDebug() << "AudioCaptor: EOF after" << packet_count << "packets";
            break;
        } else if (ret == AVERROR(EAGAIN)) continue;
        else if (ret < 0) {
            qDebug() << "AudioCaptor: av_read_frame error:" << av_error_cxx(ret).c_str();
            continue;
        }

        packet_count++;

        if (av_packet->stream_index == audio_index) {
            audio_packet_count++;
            decode_func(std::move(av_packet));

            if (audio_packet_count % 100 == 0) {
                qDebug() << "AudioCaptor: processed" << audio_packet_count
                        << "audio packets out of" << packet_count << "total";
            }
        }
    }
}

void AudioCaptor::receive_frame(AVPacketPtr obj_packet) {
    if (!queue || !av_codec_context) return;

    auto packet = std::move(obj_packet);
    int ret = 0;
    while ((ret = avcodec_send_packet(av_codec_context.get(), packet.get())) == AVERROR(EAGAIN)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "send audio packet failed: %s\n", av_error_cxx(ret).c_str());
        return;
    }

    while (ret >= 0) {
        AVFramePtr raw_frame(av_frame_alloc(), AVFrameDeleter());
        if (!raw_frame) return;
        ret = avcodec_receive_frame(av_codec_context.get(), raw_frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) continue;

        // 直接推送原始帧（不重采样）
        // 注意：增加引用计数，因为 raw_frame 会被 move 到队列
        AVFramePtr clone_frame(av_frame_clone(raw_frame.get()), AVFrameDeleter());
        if (!clone_frame) continue;

        queue->push_no_wait(std::move(clone_frame));
        notify_frame_ready();
    }
}

void AudioCaptor::notify_frame_ready() {
    static int count = 0;
    if (++count % 100 == 0) {
        qDebug() << "AudioCaptor: notify_frame_ready called" << count << "times";
    }
    std::lock_guard<std::mutex> lock(callback_mutex);
    if (frame_ready_callback) {
        frame_ready_callback();
    }
}
