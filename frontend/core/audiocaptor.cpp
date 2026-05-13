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

    // ✅ 清理：移除重采样相关代码
    // 音频帧保持设备原始格式，电平计算已改为适配任意格式

    decode_func = [this](AVPacketPtr packet) {
        receive_frame(std::move(packet));
    };

    av_dict_free(&options);

    // ✅ 添加格式信息日志
    av_log(nullptr, AV_LOG_INFO, "AudioCaptor initialized: sample_rate=%d, channels=%d, format=%d\n",
           av_codec_context->sample_rate,
           av_codec_context->ch_layout.nb_channels,
           av_codec_context->sample_fmt);

    m_initialized = true;
}

void AudioCaptor::capture_loop() {
    if (!av_format_context) {
        return;
    }

    int packet_count = 0;
    int audio_packet_count = 0;

    while (is_capturing.load()) {
        AVPacketPtr av_packet(av_packet_alloc(), AVPacketDeleter());
        int ret = av_read_frame(av_format_context.get(), av_packet.get());
        if (ret == AVERROR_EOF) {
            break;
        } else if (ret == AVERROR(EAGAIN)) continue;
        else if (ret < 0) {
            continue;
        }

        packet_count++;

        if (av_packet->stream_index == audio_index) {
            audio_packet_count++;
            decode_func(std::move(av_packet));
        }
    }
}

void AudioCaptor::receive_frame(AVPacketPtr obj_packet) {
    if (!queue || !av_codec_context) return;

    // 移动语义接管 packet 所有权
    auto packet = std::move(obj_packet);

    // 发送 packet 到解码器，处理 EAGAIN
    int ret = 0;
    while ((ret = avcodec_send_packet(av_codec_context.get(), packet.get())) == AVERROR(EAGAIN)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "send audio packet failed: %s\n", av_error_cxx(ret).c_str());
        return;
    }
    // packet 在此之后不再需要，离开作用域时自动释放

    // 循环接收所有可用的解码帧
    while (ret >= 0) {
        // ✅ 使用智能指针 + 自定义删除器
        AVFramePtr raw_frame(av_frame_alloc(), AVFrameDeleter());
        if (!raw_frame) return;

        ret = avcodec_receive_frame(av_codec_context.get(), raw_frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break; // raw_frame 自动释放
        }
        if (ret < 0) {
            continue; // raw_frame 自动释放
        }

        // ✅ 零拷贝：用 av_frame_ref 创建引用计数副本
        AVFramePtr ref_frame(av_frame_alloc(), AVFrameDeleter());
        if (!ref_frame) continue;

        ret = av_frame_ref(ref_frame.get(), raw_frame.get());
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "av_frame_ref failed: %s\n", av_error_cxx(ret).c_str());
            continue; // ref_frame 和 raw_frame 自动释放
        }

        // ✅ 移动语义：将智能指针移入队列，零拷贝交出所有权
        queue->push_no_wait(std::move(ref_frame));

        // raw_frame 离开作用域时自动释放
        // ref_frame 已被移走，变为空指针

        notify_frame_ready();
    }
    // packet 和 raw_frame 均在各自作用域结束时自动释放
}

void AudioCaptor::notify_frame_ready() {
    // ✅ 修复：移除静态变量，改用实例成员（如果要计数的话）
    // 当前简单实现：每次直接回调
    std::lock_guard<std::mutex> lock(callback_mutex);
    if (frame_ready_callback) {
        frame_ready_callback();
    }
}
