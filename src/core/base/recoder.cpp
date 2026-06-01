#include "recoder.h"

#include <algorithm>
#include <future>

Recoder::Recoder() = default;

Recoder::~Recoder() {
    stop();
}

void Recoder::start(const std::string &output_path, int canvas_w, int canvas_h, int fps,
                       DataSafeQueue<AVFramePtr> *system_audio_src,
                       DataSafeQueue<AVFramePtr> *mic_audio_src) {
    m_system_audio_src = system_audio_src;
    m_mic_audio_src = mic_audio_src;

    stop();

    m_canvas_w = canvas_w;
    m_canvas_h = canvas_h;
    m_fps = fps;

    m_video_queue = std::make_unique<DataSafeQueue<VideoFrame>>(120);
    m_recording.store(true);
    m_encode_thread = std::thread([this]() { encoding_loop(); });
}

void Recoder::stop() {
    m_recording.store(false);

    auto wake_queue = [](auto *q) {
        if (q) {
            q->set_pause_state(true);
            q->nut_empty_wake_up();
        }
    };
    wake_queue(m_video_queue.get());
    wake_queue(m_system_audio_src);
    wake_queue(m_mic_audio_src);

    if (m_encode_thread.joinable()) {
        std::packaged_task<void()> join_task([this]() {
            m_encode_thread.join();
        });
        auto future = join_task.get_future();
        std::thread(std::move(join_task)).detach();

        if (future.wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
            av_log(nullptr, AV_LOG_WARNING, "encode thread did not exit within 3s, detaching\n");
            m_encode_thread.detach();
        }
    }

    auto reset_queue = [](auto *q) {
        if (q) q->set_pause_state(false);
    };
    reset_queue(m_system_audio_src);
    reset_queue(m_mic_audio_src);

    m_video_queue.reset();
}

void Recoder::feed_frame(const uint8_t *data, int stride, int capture_w, int capture_h) {
    if (!m_recording.load() || !m_video_queue) return;
    VideoFrame frame;
    frame.data.assign(data, data + stride * capture_h);
    frame.width = capture_w;
    frame.height = capture_h;
    frame.stride = stride;
    m_video_queue->push_no_wait(std::move(frame));
}

bool Recoder::init_audio_swr(SwrContextPtr &swr, const AVFrame *frame) {
    if (swr) return true;

    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, AUDIO_CHANNELS);

    SwrContext *raw = nullptr;
    int ret = swr_alloc_set_opts2(&raw,
                                  &out_ch_layout, AUDIO_FORMAT, AUDIO_SAMPLE_RATE,
                                  &frame->ch_layout, static_cast<AVSampleFormat>(frame->format), frame->sample_rate,
                                  0, nullptr);
    SwrContextPtr tmp(raw);
    if (ret < 0 || !raw) {
        av_log(nullptr, AV_LOG_ERROR, "swr_alloc_set_opts2 failed: %s\n", av_error_cxx(ret).c_str());
        return false;
    }

    if (swr_init(tmp.get()) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "swr_init failed\n");
        return false;
    }

    swr = std::move(tmp);
    return true;
}

void Recoder::distribute_video_packet() {
    std::lock_guard<std::mutex> lock(m_channels_mutex);
    if (m_output_channels.empty()) return;
    for (auto *ch : m_output_channels) {
        AVPacketPtr clone(av_packet_alloc(), AVPacketDeleter());
        if (av_packet_ref(clone.get(), m_pkt.get()) >= 0) {
            clone->stream_index = 0;
            ch->feed_packet(std::move(clone));
        }
    }
}

void Recoder::distribute_audio_packet() {
    std::lock_guard<std::mutex> lock(m_channels_mutex);
    if (m_output_channels.empty()) return;
    for (auto *ch : m_output_channels) {
        AVPacketPtr clone(av_packet_alloc(), AVPacketDeleter());
        if (av_packet_ref(clone.get(), m_pkt.get()) >= 0) {
            clone->stream_index = 1;
            ch->feed_packet(std::move(clone));
        }
    }
}

void Recoder::encode_video_frame(int64_t pts) {
    if (!m_pkt) return;
    m_yuv_frame->pts = pts;
    int ret = avcodec_send_frame(m_video_enc_ctx.get(), m_yuv_frame.get());
    while (ret == AVERROR(EAGAIN)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ret = avcodec_send_frame(m_video_enc_ctx.get(), m_yuv_frame.get());
    }
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "video avcodec_send_frame failed: %s\n", av_error_cxx(ret).c_str());
        return;
    }
    while (avcodec_receive_packet(m_video_enc_ctx.get(), m_pkt.get()) == 0) {
        distribute_video_packet();
        av_packet_unref(m_pkt.get());
    }
}

bool Recoder::encode_audio_frame() {
    if (!m_pkt) return false;
    m_audio_frame->pts = m_audio_pts;
    int ret = avcodec_send_frame(m_audio_enc_ctx.get(), m_audio_frame.get());
    while (ret == AVERROR(EAGAIN)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ret = avcodec_send_frame(m_audio_enc_ctx.get(), m_audio_frame.get());
    }
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "audio avcodec_send_frame failed: %s\n", av_error_cxx(ret).c_str());
        return false;
    }
    while (avcodec_receive_packet(m_audio_enc_ctx.get(), m_pkt.get()) == 0) {
        distribute_audio_packet();
        av_packet_unref(m_pkt.get());
    }
    return true;
}

bool Recoder::init_codecs() {
    const AVCodec *vcodec = avcodec_find_encoder_by_name("libx264");
    if (!vcodec) vcodec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!vcodec) {
        av_log(nullptr, AV_LOG_ERROR, "no video encoder\n");
        return false;
    }

    AVCodecContextPtr video_ctx(avcodec_alloc_context3(vcodec));
    if (!video_ctx) return false;
    video_ctx->width = m_canvas_w;
    video_ctx->height = m_canvas_h;
    video_ctx->time_base = AVRational{1, m_fps};
    video_ctx->framerate = AVRational{m_fps, 1};
    video_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    video_ctx->bit_rate = 10'000'000;
    video_ctx->gop_size = m_fps * 2;
    video_ctx->max_b_frames = 0;
    {
        AVDictionary *opts = nullptr;
        av_dict_set(&opts, "preset", "veryfast", 0);
        av_dict_set(&opts, "crf", "23", 0);
        av_dict_set(&opts, "tune", "zerolatency", 0);
        int ret = avcodec_open2(video_ctx.get(), vcodec, &opts);
        av_dict_free(&opts);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "open video encoder failed: %s\n", av_error_cxx(ret).c_str());
            return false;
        }
    }
    m_video_enc_ctx = std::move(video_ctx);
    m_video_codecpar = avcodec_parameters_alloc();
    avcodec_parameters_from_context(m_video_codecpar, m_video_enc_ctx.get());

    m_has_audio = m_system_audio_src || m_mic_audio_src;
    const AVCodec *acodec = nullptr;
    if (m_has_audio) {
        static const AVCodecID audio_codec_candidates[] = {
            AV_CODEC_ID_AAC,
            AV_CODEC_ID_MP3,
            AV_CODEC_ID_OPUS,
            AV_CODEC_ID_NONE
        };
        for (int ci = 0; audio_codec_candidates[ci] != AV_CODEC_ID_NONE; ++ci) {
            acodec = avcodec_find_encoder(audio_codec_candidates[ci]);
            if (acodec) {
                av_log(nullptr, AV_LOG_INFO, "found audio encoder: %s\n", acodec->name);
                break;
            }
        }
        if (!acodec) {
            av_log(nullptr, AV_LOG_WARNING, "no audio encoder found, recording without audio\n");
            m_has_audio = false;
        } else {
            AVCodecContextPtr audio_ctx(avcodec_alloc_context3(acodec));
            if (!audio_ctx) return false;
            audio_ctx->sample_rate = AUDIO_SAMPLE_RATE;
            av_channel_layout_default(&audio_ctx->ch_layout, AUDIO_CHANNELS);
            audio_ctx->sample_fmt = acodec->sample_fmts ? acodec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
            audio_ctx->bit_rate = 128'000;
            audio_ctx->time_base = AVRational{1, AUDIO_SAMPLE_RATE};

            AVDictionary *aopts = nullptr;
            av_dict_set(&aopts, "strict", "experimental", 0);
            int a_ret = avcodec_open2(audio_ctx.get(), acodec, &aopts);
            av_dict_free(&aopts);
            if (a_ret < 0) {
                av_log(nullptr, AV_LOG_ERROR, "open audio encoder failed: %s\n", av_error_cxx(a_ret).c_str());
                m_has_audio = false;
            } else {
                m_audio_enc_ctx = std::move(audio_ctx);
                m_audio_codecpar = avcodec_parameters_alloc();
                avcodec_parameters_from_context(m_audio_codecpar, m_audio_enc_ctx.get());
            }
        }
    }

    AVFramePtr yuv_frame(av_frame_alloc(), AVFrameDeleter());
    if (!yuv_frame) return false;
    yuv_frame->width = m_canvas_w;
    yuv_frame->height = m_canvas_h;
    yuv_frame->format = AV_PIX_FMT_YUV420P;
    if (av_frame_get_buffer(yuv_frame.get(), 0) < 0) return false;
    m_yuv_frame = std::move(yuv_frame);

    m_pkt = AVPacketPtr(av_packet_alloc(), AVPacketDeleter());
    if (!m_pkt) return false;

    m_audio_frame_size = AUDIO_FRAME_SAMPLES;
    if (m_audio_enc_ctx && m_audio_enc_ctx->frame_size > 0)
        m_audio_frame_size = m_audio_enc_ctx->frame_size;

    if (m_has_audio) {
        AVFramePtr audio_frame(av_frame_alloc(), AVFrameDeleter());
        if (!audio_frame) return false;
        audio_frame->nb_samples = m_audio_frame_size;
        audio_frame->format = m_audio_enc_ctx->sample_fmt;
        audio_frame->sample_rate = AUDIO_SAMPLE_RATE;
        av_channel_layout_default(&audio_frame->ch_layout, AUDIO_CHANNELS);
        if (av_frame_get_buffer(audio_frame.get(), 0) < 0) return false;
        m_audio_frame = std::move(audio_frame);
    }

    m_last_capture_w = m_canvas_w;
    m_last_capture_h = m_canvas_h;

    m_audio_clock = 0;
    m_video_pts = 0;
    m_audio_pts = 0;
    m_audio_frames_received = 0;
    m_audio_frames_encoded = 0;
    m_sys_silence_samples = 0;
    m_mic_silence_samples = 0;

    for (int ch = 0; ch < 2; ch++) {
        m_audio_fifo[ch].clear();
        m_sys_fifo[ch].clear();
        m_mic_fifo[ch].clear();
    }

    return true;
}

void Recoder::process_system_audio() {
    while (!m_system_audio_src->is_empty()) {
        auto frame = m_system_audio_src->try_pop();
        if (!frame || !frame.value()) break;

        m_sys_silence_samples = 0;

        if (init_audio_swr(m_sys_swr, frame.value().get())) {
            AVFramePtr out(av_frame_alloc(), AVFrameDeleter());
            if (!out) continue;
            out->sample_rate = 48000;
            av_channel_layout_default(&out->ch_layout, 2);
            out->format = AV_SAMPLE_FMT_FLTP;
            out->nb_samples = av_rescale_rnd(
                swr_get_delay(m_sys_swr.get(), frame.value()->sample_rate) + frame.value()->nb_samples,
                48000, frame.value()->sample_rate, AV_ROUND_UP);
            if (av_frame_get_buffer(out.get(), 0) < 0) continue;

            int converted = swr_convert(m_sys_swr.get(), out->data, out->nb_samples,
                                        (const uint8_t **) frame.value()->data,
                                        frame.value()->nb_samples);
            if (converted > 0) {
                if (m_sys_fifo[0].size() + converted > MAX_FIFO_SIZE) {
                    av_log(nullptr, AV_LOG_WARNING, "Audio FIFO overflow, dropping old samples\n");
                    size_t drop = m_sys_fifo[0].size() + converted - MAX_FIFO_SIZE;
                    for (int ch = 0; ch < 2; ch++) {
                        m_sys_fifo[ch].erase(m_sys_fifo[ch].begin(), m_sys_fifo[ch].begin() + drop);
                    }
                }
                float *d0 = (float *) out->data[0];
                float *d1 = (float *) out->data[1];
                float sys_vol = m_system_muted.load() ? 0.0f : m_system_volume.load();
                for (int i = 0; i < converted; ++i) {
                    m_sys_fifo[0].push_back(d0[i] * sys_vol);
                    m_sys_fifo[1].push_back(d1[i] * sys_vol);
                }
            }
        }
    }
}

void Recoder::process_mic_audio() {
    while (!m_mic_audio_src->is_empty()) {
        auto frame = m_mic_audio_src->try_pop();
        if (!frame || !frame.value()) break;

        m_mic_silence_samples = 0;

        if (init_audio_swr(m_mic_swr, frame.value().get())) {
            AVFramePtr out(av_frame_alloc(), AVFrameDeleter());
            if (!out) continue;
            out->sample_rate = 48000;
            av_channel_layout_default(&out->ch_layout, 2);
            out->format = AV_SAMPLE_FMT_FLTP;
            out->nb_samples = av_rescale_rnd(
                swr_get_delay(m_mic_swr.get(), frame.value()->sample_rate) + frame.value()->nb_samples,
                48000, frame.value()->sample_rate, AV_ROUND_UP);
            if (av_frame_get_buffer(out.get(), 0) < 0) continue;

            int converted = swr_convert(m_mic_swr.get(), out->data, out->nb_samples,
                                        (const uint8_t **) frame.value()->data,
                                        frame.value()->nb_samples);
            if (converted > 0) {
                if (m_mic_fifo[0].size() + converted > MAX_FIFO_SIZE) {
                    av_log(nullptr, AV_LOG_WARNING, "Audio FIFO overflow, dropping old samples\n");
                    size_t drop = m_mic_fifo[0].size() + converted - MAX_FIFO_SIZE;
                    for (int ch = 0; ch < 2; ch++) {
                        m_mic_fifo[ch].erase(m_mic_fifo[ch].begin(), m_mic_fifo[ch].begin() + drop);
                    }
                }

                float *d0 = (float *) out->data[0];
                float *d1 = (float *) out->data[1];
                float mic_vol = m_mic_muted.load() ? 0.0f : m_mic_volume.load();
                for (int i = 0; i < converted; ++i) {
                    m_mic_fifo[0].push_back(d0[i] * mic_vol);
                    m_mic_fifo[1].push_back(d1[i] * mic_vol);
                }
            }
        }
    }
}

void Recoder::mix_audio_streams() {
    size_t mix_length = 0;
    if (!m_sys_fifo[0].empty() && !m_mic_fifo[0].empty()) {
        mix_length = std::min(m_sys_fifo[0].size(), m_mic_fifo[0].size());
    } else if (!m_sys_fifo[0].empty()) {
        mix_length = m_sys_fifo[0].size();
        for (size_t i = 0; i < mix_length; i++) {
            m_mic_fifo[0].push_back(0.0f);
            m_mic_fifo[1].push_back(0.0f);
        }
    } else if (!m_mic_fifo[0].empty()) {
        mix_length = m_mic_fifo[0].size();
        for (size_t i = 0; i < mix_length; i++) {
            m_sys_fifo[0].push_back(0.0f);
            m_sys_fifo[1].push_back(0.0f);
        }
    }

    if (mix_length > 0) {
        for (size_t i = 0; i < mix_length; i++) {
            float mix_l = m_sys_fifo[0][i] + m_mic_fifo[0][i];
            float mix_r = m_sys_fifo[1][i] + m_mic_fifo[1][i];
            mix_l = tanhf(mix_l * 1.5f) / 1.5f;
            mix_r = tanhf(mix_r * 1.5f) / 1.5f;
            m_audio_fifo[0].push_back(mix_l);
            m_audio_fifo[1].push_back(mix_r);
        }

        for (int ch = 0; ch < 2; ch++) {
            m_sys_fifo[ch].erase(m_sys_fifo[ch].begin(), m_sys_fifo[ch].begin() + mix_length);
            m_mic_fifo[ch].erase(m_mic_fifo[ch].begin(), m_mic_fifo[ch].begin() + mix_length);
        }

        m_audio_frames_received += mix_length;
    }

    m_audio_clock += mix_length;
}

void Recoder::process_video_frame() {
    auto video_data = m_video_queue ? m_video_queue->try_pop() : std::nullopt;
    if (!video_data) return;

    int cap_w = video_data->width;
    int cap_h = video_data->height;

    if (!m_sws_ctx || cap_w != m_last_capture_w || cap_h != m_last_capture_h) {
        m_sws_ctx.reset(sws_getContext(cap_w, cap_h, AV_PIX_FMT_BGRA,
                                       m_canvas_w, m_canvas_h, AV_PIX_FMT_YUV420P,
                                       SWS_BILINEAR, nullptr, nullptr, nullptr));
        m_last_capture_w = cap_w;
        m_last_capture_h = cap_h;
        if (!m_sws_ctx) {
            av_log(nullptr, AV_LOG_ERROR, "sws_getContext failed\n");
            return;
        }
    }

    uint8_t *src_slice[1] = {video_data->data.data()};
    int src_stride[1] = {video_data->stride};
    sws_scale(m_sws_ctx.get(), src_slice, src_stride, 0, cap_h,
              m_yuv_frame->data, m_yuv_frame->linesize);

    m_video_pts++;
    if (m_has_audio && m_audio_enc_ctx) {
        int64_t expected_pts = m_audio_clock * m_fps / AUDIO_SAMPLE_RATE;
        if (expected_pts > m_video_pts) {
            m_video_pts = expected_pts;
        }
    }

    encode_video_frame(m_video_pts);
}

void Recoder::encode_audio_frames_from_fifo() {
    if (!m_audio_frame) return;
    while ((int) m_audio_fifo[0].size() >= m_audio_frame_size) {
        int take = m_audio_frame_size;
        float *dst0 = (float *) m_audio_frame->data[0];
        float *dst1 = (float *) m_audio_frame->data[1];

        for (int j = 0; j < take; ++j) {
            dst0[j] = m_audio_fifo[0][j];
            dst1[j] = m_audio_fifo[1][j];
        }

        if (encode_audio_frame()) {
            m_audio_pts += take;
            m_audio_fifo[0].erase(m_audio_fifo[0].begin(), m_audio_fifo[0].begin() + take);
            m_audio_fifo[1].erase(m_audio_fifo[1].begin(), m_audio_fifo[1].begin() + take);
            m_audio_frames_encoded++;
        } else {
            break;
        }
    }
}

void Recoder::main_encode_loop() {
    int iter_counter = 0;
    while (m_recording.load()) {
        bool did_work = false;

        if (m_has_audio && m_system_audio_src) {
            size_t before = m_sys_fifo[0].size();
            process_system_audio();
            if (m_sys_fifo[0].size() != before) did_work = true;
        }

        if (m_has_audio && m_mic_audio_src) {
            size_t before = m_mic_fifo[0].size();
            process_mic_audio();
            if (m_mic_fifo[0].size() != before) did_work = true;
        }

        if (m_has_audio) {
            size_t before = m_audio_fifo[0].size();
            mix_audio_streams();
            if (m_audio_fifo[0].size() != before) did_work = true;
        }

        {
            size_t before = m_video_queue ? m_video_queue->get_queue_size() : 0;
            process_video_frame();
            if (!m_video_queue || m_video_queue->get_queue_size() != before) did_work = true;
        }

        if (m_has_audio) {
            size_t before = m_audio_fifo[0].size();
            encode_audio_frames_from_fifo();
            if (m_audio_fifo[0].size() != before) did_work = true;
        }

        if (++iter_counter % 150 == 0) {
            av_log(nullptr, AV_LOG_INFO,
                   "encoding: audio_clock=%ld video_pts=%ld a_fifo=%zu sys=%zu mic=%zu a_recv=%d a_enc=%d\n",
                   m_audio_clock, m_video_pts,
                   m_audio_fifo[0].size(), m_sys_fifo[0].size(), m_mic_fifo[0].size(),
                   m_audio_frames_received, m_audio_frames_encoded);
        }

        if (!did_work) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void Recoder::drain_video_frames() {
    while (m_video_queue && !m_video_queue->is_empty()) {
        auto video_data = m_video_queue->try_pop();
        if (!video_data) break;

        int cap_w = video_data->width;
        int cap_h = video_data->height;

        if (!m_sws_ctx || cap_w != m_last_capture_w || cap_h != m_last_capture_h) {
            m_sws_ctx.reset(sws_getContext(cap_w, cap_h, AV_PIX_FMT_BGRA,
                                           m_canvas_w, m_canvas_h, AV_PIX_FMT_YUV420P,
                                           SWS_BILINEAR, nullptr, nullptr, nullptr));
            m_last_capture_w = cap_w;
            m_last_capture_h = cap_h;
            if (!m_sws_ctx) break;
        }

        uint8_t *src_slice[1] = {video_data->data.data()};
        int src_stride[1] = {video_data->stride};
        sws_scale(m_sws_ctx.get(), src_slice, src_stride, 0, cap_h,
                  m_yuv_frame->data, m_yuv_frame->linesize);

        encode_video_frame(m_video_pts);

        m_video_pts++;
        if (m_has_audio && m_audio_enc_ctx) {
            int64_t expected_pts = m_audio_clock * m_fps / AUDIO_SAMPLE_RATE;
            if (expected_pts > m_video_pts) {
                m_video_pts = expected_pts;
            }
        }
    }
}

void Recoder::drain_audio_residual() {
    if (!m_has_audio) return;

    auto flush_swr = [&](SwrContextPtr &swr, std::deque<float> *fifo) {
        if (!swr) return;
        AVFramePtr flush_frame(av_frame_alloc(), AVFrameDeleter());
        if (!flush_frame) return;
        flush_frame->sample_rate = AUDIO_SAMPLE_RATE;
        av_channel_layout_default(&flush_frame->ch_layout, AUDIO_CHANNELS);
        flush_frame->format = AUDIO_FORMAT;
        int delay_samples = swr_get_delay(swr.get(), AUDIO_SAMPLE_RATE);
        if (delay_samples <= 0) return;
        flush_frame->nb_samples = delay_samples;
        if (av_frame_get_buffer(flush_frame.get(), 0) < 0) return;
        int converted = swr_convert(swr.get(), flush_frame->data, flush_frame->nb_samples, nullptr, 0);
        if (converted > 0) {
            float *d0 = (float *) flush_frame->data[0];
            float *d1 = (float *) flush_frame->data[1];
            for (int i = 0; i < converted; ++i) {
                fifo[0].push_back(d0[i]);
                fifo[1].push_back(d1[i]);
            }
        }
    };

    flush_swr(m_sys_swr, m_sys_fifo);
    flush_swr(m_mic_swr, m_mic_fifo);

    while (!m_sys_fifo[0].empty() || !m_mic_fifo[0].empty()) {
        size_t max_len = std::max(m_sys_fifo[0].size(), m_mic_fifo[0].size());
        while (m_sys_fifo[0].size() < max_len) {
            m_sys_fifo[0].push_back(0.0f);
            m_sys_fifo[1].push_back(0.0f);
        }
        while (m_mic_fifo[0].size() < max_len) {
            m_mic_fifo[0].push_back(0.0f);
            m_mic_fifo[1].push_back(0.0f);
        }

        for (size_t i = 0; i < max_len; i++) {
            float mix_l = m_sys_fifo[0][i] + m_mic_fifo[0][i];
            float mix_r = m_sys_fifo[1][i] + m_mic_fifo[1][i];
            mix_l = tanhf(mix_l * 1.5f) / 1.5f;
            mix_r = tanhf(mix_r * 1.5f) / 1.5f;
            m_audio_fifo[0].push_back(mix_l);
            m_audio_fifo[1].push_back(mix_r);
        }

        m_sys_fifo[0].clear();
        m_sys_fifo[1].clear();
        m_mic_fifo[0].clear();
        m_mic_fifo[1].clear();
    }

    if (m_audio_frame && !m_audio_fifo[0].empty()) {
        while (!m_audio_fifo[0].empty()) {
            int remaining = (int) m_audio_fifo[0].size();
            int take = std::min(remaining, m_audio_frame_size);

            float *dst0 = (float *) m_audio_frame->data[0];
            float *dst1 = (float *) m_audio_frame->data[1];

            for (int j = 0; j < take; ++j) {
                dst0[j] = m_audio_fifo[0][j];
                dst1[j] = m_audio_fifo[1][j];
            }

            for (int j = take; j < m_audio_frame_size; ++j) {
                dst0[j] = 0.0f;
                dst1[j] = 0.0f;
            }

            if (encode_audio_frame()) {
                m_audio_pts += take;
                m_audio_fifo[0].erase(m_audio_fifo[0].begin(),
                                      m_audio_fifo[0].begin() + std::min(take, (int) m_audio_fifo[0].size()));
                m_audio_fifo[1].erase(m_audio_fifo[1].begin(),
                                      m_audio_fifo[1].begin() + std::min(take, (int) m_audio_fifo[1].size()));
                m_audio_frames_encoded++;
            }

            if (take < m_audio_frame_size) break;
        }
    }
}

void Recoder::flush_encoders() {
    avcodec_send_frame(m_video_enc_ctx.get(), nullptr);
    while (avcodec_receive_packet(m_video_enc_ctx.get(), m_pkt.get()) == 0) {
        distribute_video_packet();
        av_packet_unref(m_pkt.get());
    }

    if (m_has_audio && m_audio_enc_ctx) {
        avcodec_send_frame(m_audio_enc_ctx.get(), nullptr);
        while (avcodec_receive_packet(m_audio_enc_ctx.get(), m_pkt.get()) == 0) {
            distribute_audio_packet();
            av_packet_unref(m_pkt.get());
        }
    }
}

void Recoder::reset_state() {
    m_video_enc_ctx.reset();
    avcodec_parameters_free(&m_video_codecpar);
    m_audio_enc_ctx.reset();
    avcodec_parameters_free(&m_audio_codecpar);
    m_sws_ctx.reset();
    m_sys_swr.reset();
    m_mic_swr.reset();
    m_yuv_frame.reset();
    m_audio_frame.reset();
    m_pkt.reset();

    m_has_audio = false;
    m_audio_frame_size = AUDIO_FRAME_SAMPLES;
    m_last_capture_w = 0;
    m_last_capture_h = 0;

    m_audio_clock = 0;
    m_video_pts = 0;
    m_audio_pts = 0;
    m_audio_frames_received = 0;
    m_audio_frames_encoded = 0;
    m_sys_silence_samples = 0;
    m_mic_silence_samples = 0;

    for (int ch = 0; ch < 2; ch++) {
        std::deque<float>().swap(m_audio_fifo[ch]);
        std::deque<float>().swap(m_sys_fifo[ch]);
        std::deque<float>().swap(m_mic_fifo[ch]);
    }

    if (m_video_queue) m_video_queue->clean_queue();
}

void Recoder::encoding_loop() {
    if (!init_codecs()) {
        av_log(nullptr, AV_LOG_ERROR, "recording failed\n");
        reset_state();
        return;
    }

    main_encode_loop();

    drain_video_frames();
    drain_audio_residual();
    flush_encoders();

    reset_state();
}

void Recoder::register_output(OutputChannel *channel) {
    std::lock_guard<std::mutex> lock(m_channels_mutex);
    if (channel && std::find(m_output_channels.begin(), m_output_channels.end(), channel) == m_output_channels.end()) {
        m_output_channels.push_back(channel);
    }
}

void Recoder::unregister_output(OutputChannel *channel) {
    std::lock_guard<std::mutex> lock(m_channels_mutex);
    auto it = std::find(m_output_channels.begin(), m_output_channels.end(), channel);
    if (it != m_output_channels.end()) {
        m_output_channels.erase(it);
    }
}

size_t Recoder::output_count() const {
    std::lock_guard<std::mutex> lock(m_channels_mutex);
    return m_output_channels.size();
}

AVCodecParameters *Recoder::video_codecpar() const {
    return m_video_codecpar;
}

AVCodecParameters *Recoder::audio_codecpar() const {
    return m_audio_codecpar;
}

AVRational Recoder::video_time_base() const {
    if (m_video_enc_ctx) return m_video_enc_ctx->time_base;
    return AVRational{1, 30};
}

AVRational Recoder::audio_time_base() const {
    if (m_audio_enc_ctx) return m_audio_enc_ctx->time_base;
    return AVRational{1, 48000};
}
