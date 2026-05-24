#include "recoder.h"
#include "../utils/av_err2str_cxx.h"

Recoder::Recoder() = default;

Recoder::~Recoder() {
    stop();
}

void Recoder::start(const QString &output_path, int canvas_w, int canvas_h, int fps,
                    DataSafeQueue<AVFramePtr> *system_audio_src,
                    DataSafeQueue<AVFramePtr> *mic_audio_src) {
    stop();

    m_output_path = output_path;
    m_canvas_w = canvas_w;
    m_canvas_h = canvas_h;
    m_fps = fps;
    m_system_audio_src = system_audio_src;
    m_mic_audio_src = mic_audio_src;

    m_video_queue = std::make_unique<DataSafeQueue<VideoFrame> >(120);
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
        m_encode_thread.join();
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

// =====================================================================
//  encoder setup
// =====================================================================

bool Recoder::init_video_encoder() {
    const AVCodec *vcodec = avcodec_find_encoder_by_name("libx264");
    if (!vcodec) vcodec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!vcodec) {
        av_log(nullptr, AV_LOG_ERROR, "no video encoder\n");
        return false;
    }

    AVCodecContext *raw = avcodec_alloc_context3(vcodec);
    if (!raw) return false;
    m_video_enc_ctx.reset(raw);

    m_video_enc_ctx->width = m_canvas_w;
    m_video_enc_ctx->height = m_canvas_h;
    m_video_enc_ctx->time_base = AVRational{1, m_fps};
    m_video_enc_ctx->framerate = AVRational{m_fps, 1};
    m_video_enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    m_video_enc_ctx->bit_rate = 10'000'000;
    m_video_enc_ctx->gop_size = m_fps * 2;
    m_video_enc_ctx->max_b_frames = 0;
    if (m_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        m_video_enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "preset", "veryfast", 0);
    av_dict_set(&opts, "crf", "23", 0);
    av_dict_set(&opts, "tune", "zerolatency", 0);
    int ret = avcodec_open2(m_video_enc_ctx.get(), vcodec, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "open video encoder failed: %s\n", av_error_cxx(ret).c_str());
        return false;
    }

    m_video_stream = avformat_new_stream(m_fmt_ctx.get(), nullptr);
    if (!m_video_stream) return false;
    m_video_stream->time_base = m_video_enc_ctx->time_base;
    m_video_stream->id = 0;
    avcodec_parameters_from_context(m_video_stream->codecpar, m_video_enc_ctx.get());
    return true;
}

bool Recoder::init_audio_encoder() {
    static const AVCodecID audio_codec_candidates[] = {
        AV_CODEC_ID_AAC, AV_CODEC_ID_MP3, AV_CODEC_ID_OPUS, AV_CODEC_ID_NONE
    };
    const AVCodec *acodec = nullptr;
    for (int ci = 0; audio_codec_candidates[ci] != AV_CODEC_ID_NONE; ++ci) {
        acodec = avcodec_find_encoder(audio_codec_candidates[ci]);
        if (acodec) {
            av_log(nullptr, AV_LOG_INFO, "found audio encoder: %s\n", acodec->name);
            break;
        }
    }
    if (!acodec) {
        av_log(nullptr, AV_LOG_WARNING, "no audio encoder found, recording without audio\n");
        return false;
    }

    AVCodecContext *raw = avcodec_alloc_context3(acodec);
    if (!raw) return false;
    m_audio_enc_ctx.reset(raw);

    m_audio_enc_ctx->sample_rate = AUDIO_SAMPLE_RATE;
    av_channel_layout_default(&m_audio_enc_ctx->ch_layout, AUDIO_CHANNELS);
    m_audio_enc_ctx->sample_fmt = acodec->sample_fmts ? acodec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
    m_audio_enc_ctx->bit_rate = 128'000;
    m_audio_enc_ctx->time_base = AVRational{1, AUDIO_SAMPLE_RATE};
    if (m_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        m_audio_enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    AVDictionary *aopts = nullptr;
    av_dict_set(&aopts, "strict", "experimental", 0);
    int a_ret = avcodec_open2(m_audio_enc_ctx.get(), acodec, &aopts);
    av_dict_free(&aopts);
    if (a_ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "open audio encoder failed: %s\n", av_error_cxx(a_ret).c_str());
        m_audio_enc_ctx.reset();
        return false;
    }

    m_audio_stream = avformat_new_stream(m_fmt_ctx.get(), nullptr);
    if (!m_audio_stream) return false;
    m_audio_stream->time_base = m_audio_enc_ctx->time_base;
    m_audio_stream->id = 1;
    avcodec_parameters_from_context(m_audio_stream->codecpar, m_audio_enc_ctx.get());
    av_log(nullptr, AV_LOG_INFO,
           "audio encoder ready: %s, sample_fmt=%d, sample_rate=%d, channels=%d\n",
           acodec->name, m_audio_enc_ctx->sample_fmt,
           m_audio_enc_ctx->sample_rate, m_audio_enc_ctx->ch_layout.nb_channels);
    return true;
}

bool Recoder::open_output_and_allocate_frames() {
    if (avio_open(&m_fmt_ctx->pb, m_output_path.toUtf8().constData(), AVIO_FLAG_WRITE) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "open output file failed\n");
        return false;
    }
    if (avformat_write_header(m_fmt_ctx.get(), nullptr) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "write header failed\n");
        return false;
    }

    m_yuv_frame = AVFramePtr(av_frame_alloc(), AVFrameDeleter());
    if (!m_yuv_frame) return false;
    m_yuv_frame->width = m_canvas_w;
    m_yuv_frame->height = m_canvas_h;
    m_yuv_frame->format = AV_PIX_FMT_YUV420P;
    if (av_frame_get_buffer(m_yuv_frame.get(), 0) < 0) return false;

    m_pkt = AVPacketPtr(av_packet_alloc(), [](AVPacket *p) { av_packet_free(&p); });
    if (!m_pkt) return false;

    if (m_has_audio) {
        m_audio_frame_size = AUDIO_FRAME_SAMPLES;
        if (m_audio_enc_ctx && m_audio_enc_ctx->frame_size > 0)
            m_audio_frame_size = m_audio_enc_ctx->frame_size;

        m_audio_frame = AVFramePtr(av_frame_alloc(), AVFrameDeleter());
        if (m_audio_frame) {
            m_audio_frame->nb_samples = m_audio_frame_size;
            m_audio_frame->format = m_audio_enc_ctx->sample_fmt;
            m_audio_frame->sample_rate = AUDIO_SAMPLE_RATE;
            av_channel_layout_default(&m_audio_frame->ch_layout, AUDIO_CHANNELS);
            if (av_frame_get_buffer(m_audio_frame.get(), 0) < 0)
                m_audio_frame.reset();
        }
    }
    return true;
}

// =====================================================================
//  audio processing helpers
// =====================================================================

void Recoder::handle_audio_source(DataSafeQueue<AVFramePtr> *src,
                                   SwrContextPtr &swr,
                                   std::deque<float> *fifo,
                                   int64_t &silence_counter,
                                   bool &did_work) {
    while (!src->is_empty()) {
        auto frame = src->try_pop();
        if (!frame || !frame.value()) break;
        did_work = true;
        silence_counter = 0;

        if (!init_audio_swr(swr, frame.value().get())) continue;

        AVFramePtr out(av_frame_alloc(), AVFrameDeleter());
        if (!out) continue;
        out->sample_rate = AUDIO_SAMPLE_RATE;
        av_channel_layout_default(&out->ch_layout, AUDIO_CHANNELS);
        out->format = AV_SAMPLE_FMT_FLTP;
        out->nb_samples = av_rescale_rnd(
            swr_get_delay(swr.get(), frame.value()->sample_rate) + frame.value()->nb_samples,
            AUDIO_SAMPLE_RATE, frame.value()->sample_rate, AV_ROUND_UP);
        if (av_frame_get_buffer(out.get(), 0) < 0) continue;

        int converted = swr_convert(swr.get(), out->data, out->nb_samples,
                                    (const uint8_t **) frame.value()->data,
                                    frame.value()->nb_samples);
        if (converted <= 0) continue;

        if (fifo[0].size() + converted > MAX_FIFO_SIZE) {
            av_log(nullptr, AV_LOG_WARNING, "Audio FIFO overflow, dropping old samples\n");
            size_t drop = fifo[0].size() + converted - MAX_FIFO_SIZE;
            for (int ch = 0; ch < AUDIO_CHANNELS; ch++) {
                fifo[ch].erase(fifo[ch].begin(), fifo[ch].begin() + drop);
            }
        }

        float *d0 = (float *) out->data[0];
        float *d1 = (float *) out->data[1];
        for (int i = 0; i < converted; ++i) {
            fifo[0].push_back(d0[i] * 0.7f);
            fifo[1].push_back(d1[i] * 0.7f);
        }
    }
}

size_t Recoder::mix_audio_sources() {
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
        for (int ch = 0; ch < AUDIO_CHANNELS; ch++) {
            m_sys_fifo[ch].erase(m_sys_fifo[ch].begin(), m_sys_fifo[ch].begin() + mix_length);
            m_mic_fifo[ch].erase(m_mic_fifo[ch].begin(), m_mic_fifo[ch].begin() + mix_length);
        }
        m_audio_frames_received += mix_length;
        m_audio_clock += mix_length;
    }

    return mix_length;
}

void Recoder::encode_audio_fifo() {
    if (!m_audio_frame) return;
    while ((int) m_audio_fifo[0].size() >= m_audio_frame_size) {
        float *dst0 = (float *) m_audio_frame->data[0];
        float *dst1 = (float *) m_audio_frame->data[1];
        for (int j = 0; j < m_audio_frame_size; ++j) {
            dst0[j] = m_audio_fifo[0][j];
            dst1[j] = m_audio_fifo[1][j];
        }
        if (encode_audio_frame(m_audio_pts)) {
            m_audio_pts += m_audio_frame_size;
            m_audio_fifo[0].erase(m_audio_fifo[0].begin(), m_audio_fifo[0].begin() + m_audio_frame_size);
            m_audio_fifo[1].erase(m_audio_fifo[1].begin(), m_audio_fifo[1].begin() + m_audio_frame_size);
            m_audio_frames_encoded++;
        } else {
            break;
        }
    }
}

// =====================================================================
//  video processing
// =====================================================================

void Recoder::handle_video_frame(const VideoFrame &video_data) {
    int cap_w = video_data.width;
    int cap_h = video_data.height;

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

    uint8_t *src_slice[1] = {const_cast<uint8_t *>(video_data.data.data())};
    int src_stride[1] = {video_data.stride};
    sws_scale(m_sws_ctx.get(), src_slice, src_stride, 0, cap_h,
              m_yuv_frame->data, m_yuv_frame->linesize);

    if (m_has_audio && m_audio_enc_ctx) {
        m_video_pts = m_audio_clock * m_fps / AUDIO_SAMPLE_RATE;
    } else {
        m_video_pts++;
    }

    encode_video_frame(m_video_pts);
}

// =====================================================================
//  drain / flush
// =====================================================================

void Recoder::drain_video_queue() {
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

        uint8_t *src_slice[1] = {const_cast<uint8_t *>(video_data->data.data())};
        int src_stride[1] = {video_data->stride};
        sws_scale(m_sws_ctx.get(), src_slice, src_stride, 0, cap_h,
                  m_yuv_frame->data, m_yuv_frame->linesize);

        encode_video_frame(m_video_pts);
        m_video_pts++;
    }
}

void Recoder::flush_swr_and_mix_remaining() {
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
}

void Recoder::drain_audio_fifo_remaining() {
    if (!m_audio_frame || m_audio_fifo[0].empty()) return;

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

        if (encode_audio_frame(m_audio_pts)) {
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

void Recoder::flush_encoders() {
    auto flush_one = [&](AVCodecContextPtr &enc_ctx, AVStream *stream) {
        if (!enc_ctx) return;
        avcodec_send_frame(enc_ctx.get(), nullptr);
        while (avcodec_receive_packet(enc_ctx.get(), m_pkt.get()) == 0) {
            av_packet_rescale_ts(m_pkt.get(), enc_ctx->time_base, stream->time_base);
            m_pkt->stream_index = stream->index;
            if (av_interleaved_write_frame(m_fmt_ctx.get(), m_pkt.get()) < 0) {
                av_log(nullptr, AV_LOG_ERROR, "flush av_interleaved_write_frame failed\n");
            }
            av_packet_unref(m_pkt.get());
        }
    };
    flush_one(m_video_enc_ctx, m_video_stream);
    flush_one(m_audio_enc_ctx, m_audio_stream);
}

// =====================================================================
//  audio SWR init (unchanged)
// =====================================================================

bool Recoder::init_audio_swr(SwrContextPtr &swr, const AVFrame *frame) {
    if (swr) return true;

    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, AUDIO_CHANNELS);

    SwrContext *raw = nullptr;
    int ret = swr_alloc_set_opts2(&raw,
                                  &out_ch_layout, AUDIO_FORMAT, AUDIO_SAMPLE_RATE,
                                  &frame->ch_layout, static_cast<AVSampleFormat>(frame->format), frame->sample_rate,
                                  0, nullptr);
    if (ret < 0 || !raw) {
        av_log(nullptr, AV_LOG_ERROR, "swr_alloc_set_opts2 failed: %s\n", av_error_cxx(ret).c_str());
        return false;
    }

    if (swr_init(raw) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "swr_init failed\n");
        swr_free(&raw);
        return false;
    }

    swr.reset(raw);
    return true;
}

// =====================================================================
//  low-level encode helpers (unchanged)
// =====================================================================

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
        av_packet_rescale_ts(m_pkt.get(), m_video_enc_ctx->time_base, m_video_stream->time_base);
        m_pkt->stream_index = m_video_stream->index;
        if (av_interleaved_write_frame(m_fmt_ctx.get(), m_pkt.get()) < 0) {
            av_log(nullptr, AV_LOG_ERROR, "video av_interleaved_write_frame failed\n");
        }
        av_packet_unref(m_pkt.get());
    }
}

bool Recoder::encode_audio_frame(int64_t audio_pts) {
    if (!m_pkt) return false;
    m_audio_frame->pts = audio_pts;
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
        av_packet_rescale_ts(m_pkt.get(), m_audio_enc_ctx->time_base, m_audio_stream->time_base);
        m_pkt->stream_index = m_audio_stream->index;
        if (av_interleaved_write_frame(m_fmt_ctx.get(), m_pkt.get()) < 0) {
            av_log(nullptr, AV_LOG_ERROR, "audio av_interleaved_write_frame failed\n");
        }
        av_packet_unref(m_pkt.get());
    }
    return true;
}

// =====================================================================
//  encoding_loop — orchestrates the full recording pipeline
// =====================================================================

void Recoder::encoding_loop() {
    m_has_audio = m_system_audio_src || m_mic_audio_src;
    bool success = false;

    do {
        // ---- reset state ----
        m_video_enc_ctx.reset();
        m_audio_enc_ctx.reset();
        m_sws_ctx.reset();
        m_sys_swr.reset();
        m_mic_swr.reset();
        m_yuv_frame.reset();
        m_audio_frame.reset();
        m_pkt.reset();
        m_video_stream = nullptr;
        m_audio_stream = nullptr;

        for (auto &ch : m_audio_fifo) ch.clear();
        for (auto &ch : m_sys_fifo) ch.clear();
        for (auto &ch : m_mic_fifo) ch.clear();

        m_audio_clock = 0;
        m_video_pts = 0;
        m_audio_pts = 0;
        m_audio_frames_received = 0;
        m_audio_frames_encoded = 0;
        m_sys_silence_samples = 0;
        m_mic_silence_samples = 0;
        m_last_capture_w = m_canvas_w;
        m_last_capture_h = m_canvas_h;
        m_audio_frame_size = AUDIO_FRAME_SAMPLES;

        // ---- setup ----
        AVFormatContext *raw_fmt = nullptr;
        avformat_alloc_output_context2(&raw_fmt, nullptr, "mp4",
                                       m_output_path.toUtf8().constData());
        if (!raw_fmt) {
            av_log(nullptr, AV_LOG_ERROR, "alloc output context failed\n");
            break;
        }
        m_fmt_ctx.reset(raw_fmt);

        if (!init_video_encoder()) break;
        if (m_has_audio) {
            m_has_audio = init_audio_encoder();
        }
        if (!open_output_and_allocate_frames()) break;

        // ---- main encoding loop ----
        while (m_recording.load()) {
            bool did_work = false;

            if (m_has_audio) {
                if (m_system_audio_src) {
                    handle_audio_source(m_system_audio_src, m_sys_swr, m_sys_fifo,
                                        m_sys_silence_samples, did_work);
                }
                if (m_mic_audio_src) {
                    handle_audio_source(m_mic_audio_src, m_mic_swr, m_mic_fifo,
                                        m_mic_silence_samples, did_work);
                }
                if (mix_audio_sources() > 0) {
                    did_work = true;
                }
                encode_audio_fifo();
            }

            auto video_data = m_video_queue ? m_video_queue->try_pop() : std::nullopt;
            if (video_data) {
                did_work = true;
                handle_video_frame(*video_data);
            }

            if (!did_work) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        // ---- drain ----
        drain_video_queue();
        if (m_has_audio) {
            flush_swr_and_mix_remaining();
            drain_audio_fifo_remaining();
        }
        flush_encoders();

        av_write_trailer(m_fmt_ctx.get());
        success = true;
    } while (false);

    if (!success)
        av_log(nullptr, AV_LOG_ERROR, "recording failed\n");

    // ---- cleanup (smart pointers handle FFmpeg resources) ----
    m_fmt_ctx.reset();
    m_video_enc_ctx.reset();
    m_audio_enc_ctx.reset();
    m_sws_ctx.reset();
    m_sys_swr.reset();
    m_mic_swr.reset();
    m_yuv_frame.reset();
    m_audio_frame.reset();
    m_pkt.reset();
    m_video_stream = nullptr;
    m_audio_stream = nullptr;

    for (auto &ch : m_audio_fifo) ch.clear();
    for (auto &ch : m_sys_fifo) ch.clear();
    for (auto &ch : m_mic_fifo) ch.clear();

    if (m_video_queue) m_video_queue->clean_queue();
}
