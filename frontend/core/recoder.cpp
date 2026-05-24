#include "recoder.h"
#include "../utils/av_err2str_cxx.h"
#include <deque>

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

    // 唤醒所有阻塞的 try_pop，但不设置 stop_request（否则 drain 阶段无法读取剩余帧）
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

    // 恢复音频队列状态（视频队列即将被 reset，不需要恢复）
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

bool Recoder::init_audio_swr(SwrContext *&swr, const AVFrame *frame, int out_sample_rate) {
    if (swr) return true;

    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, AUDIO_CHANNELS);

    int ret = swr_alloc_set_opts2(&swr,
                                  &out_ch_layout, AUDIO_FORMAT, out_sample_rate,
                                  &frame->ch_layout, static_cast<AVSampleFormat>(frame->format), frame->sample_rate,
                                  0, nullptr);
    if (ret < 0 || !swr) {
        av_log(nullptr, AV_LOG_ERROR, "swr_alloc_set_opts2 failed: %s\n", av_error_cxx(ret).c_str());
        return false;
    }

    if (swr_init(swr) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "swr_init failed\n");
        swr_free(&swr);
        return false;
    }
    return true;
}

void Recoder::encode_video_frame(AVFormatContext *fmt_ctx, AVPacket *pkt,
                                 AVCodecContext *video_enc_ctx, AVStream *video_stream,
                                 AVFrame *yuv_frame, int64_t pts) {
    if (!pkt) return;
    yuv_frame->pts = pts;
    int ret = avcodec_send_frame(video_enc_ctx, yuv_frame);
    while (ret == AVERROR(EAGAIN)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ret = avcodec_send_frame(video_enc_ctx, yuv_frame);
    }
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "video avcodec_send_frame failed: %s\n", av_error_cxx(ret).c_str());
        return;
    }
    while (avcodec_receive_packet(video_enc_ctx, pkt) == 0) {
        av_packet_rescale_ts(pkt, video_enc_ctx->time_base, video_stream->time_base);
        pkt->stream_index = video_stream->index;
        if (av_interleaved_write_frame(fmt_ctx, pkt) < 0) {
            av_log(nullptr, AV_LOG_ERROR, "video av_interleaved_write_frame failed\n");
        }
        av_packet_unref(pkt);
    }
}

bool Recoder::encode_audio_frame(AVFormatContext *fmt_ctx, AVPacket *pkt,
                                 AVCodecContext *audio_enc_ctx, AVStream *audio_stream,
                                 AVFrame *audio_frame, int64_t &audio_pts) {
    if (!pkt) return false;
    audio_frame->pts = audio_pts;
    int ret = avcodec_send_frame(audio_enc_ctx, audio_frame);
    while (ret == AVERROR(EAGAIN)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ret = avcodec_send_frame(audio_enc_ctx, audio_frame);
    }
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "audio avcodec_send_frame failed: %s\n", av_error_cxx(ret).c_str());
        return false;
    }
    while (avcodec_receive_packet(audio_enc_ctx, pkt) == 0) {
        av_packet_rescale_ts(pkt, audio_enc_ctx->time_base, audio_stream->time_base);
        pkt->stream_index = audio_stream->index;
        if (av_interleaved_write_frame(fmt_ctx, pkt) < 0) {
            av_log(nullptr, AV_LOG_ERROR, "audio av_interleaved_write_frame failed\n");
        }
        av_packet_unref(pkt);
    }
    return true;
}

bool Recoder::setup_recording(AVFormatContext *&fmt_ctx,
                              AVCodecContext *&video_enc_ctx, AVStream *&video_stream,
                              AVCodecContext *&audio_enc_ctx, AVStream *&audio_stream,
                              bool &has_audio, int &audio_frame_size,
                              AVFrame *&yuv_frame, AVFrame *&audio_frame, AVPacket *&pkt) {
    avformat_alloc_output_context2(&fmt_ctx, nullptr, "mp4",
                                   m_output_path.toUtf8().constData());
    if (!fmt_ctx) {
        av_log(nullptr, AV_LOG_ERROR, "alloc output context failed\n");
        return false;
    }

    // ========== Video encoder ==========
    const AVCodec *vcodec = avcodec_find_encoder_by_name("libx264");
    if (!vcodec) vcodec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!vcodec) {
        av_log(nullptr, AV_LOG_ERROR, "no video encoder\n");
        return false;
    }

    video_enc_ctx = avcodec_alloc_context3(vcodec);
    if (!video_enc_ctx) return false;
    video_enc_ctx->width = m_canvas_w;
    video_enc_ctx->height = m_canvas_h;
    video_enc_ctx->time_base = AVRational{1, m_fps};
    video_enc_ctx->framerate = AVRational{m_fps, 1};
    video_enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    video_enc_ctx->bit_rate = 10'000'000;
    video_enc_ctx->gop_size = m_fps * 2;
    video_enc_ctx->max_b_frames = 0;
    if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        video_enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; {
        AVDictionary *opts = nullptr;
        av_dict_set(&opts, "preset", "veryfast", 0);
        av_dict_set(&opts, "crf", "23", 0);
        av_dict_set(&opts, "tune", "zerolatency", 0);
        int ret = avcodec_open2(video_enc_ctx, vcodec, &opts);
        av_dict_free(&opts);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_ERROR, "open video encoder failed: %s\n", av_error_cxx(ret).c_str());
            return false;
        }
    }

    video_stream = avformat_new_stream(fmt_ctx, nullptr);
    if (!video_stream) return false;
    video_stream->time_base = video_enc_ctx->time_base;
    video_stream->id = 0;
    avcodec_parameters_from_context(video_stream->codecpar, video_enc_ctx);

    // ========== Audio encoder (try multiple codecs) ==========
    const AVCodec *acodec = nullptr;
    if (has_audio) {
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
            has_audio = false;
        } else {
            audio_enc_ctx = avcodec_alloc_context3(acodec);
            if (!audio_enc_ctx) return false;
            audio_enc_ctx->sample_rate = AUDIO_SAMPLE_RATE;
            av_channel_layout_default(&audio_enc_ctx->ch_layout, AUDIO_CHANNELS);
            audio_enc_ctx->sample_fmt = acodec->sample_fmts ? acodec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
            audio_enc_ctx->bit_rate = 128'000;
            audio_enc_ctx->time_base = AVRational{1, AUDIO_SAMPLE_RATE};
            if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                audio_enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            AVDictionary *aopts = nullptr;
            av_dict_set(&aopts, "strict", "experimental", 0);
            int a_ret = avcodec_open2(audio_enc_ctx, acodec, &aopts);
            av_dict_free(&aopts);
            if (a_ret < 0) {
                av_log(nullptr, AV_LOG_ERROR, "open audio encoder failed: %s\n", av_error_cxx(a_ret).c_str());
                avcodec_free_context(&audio_enc_ctx);
                audio_enc_ctx = nullptr;
                has_audio = false;
            }
        }

        if (audio_enc_ctx) {
            audio_stream = avformat_new_stream(fmt_ctx, nullptr);
            if (!audio_stream) return false;
            audio_stream->time_base = audio_enc_ctx->time_base;
            audio_stream->id = 1;
            avcodec_parameters_from_context(audio_stream->codecpar, audio_enc_ctx);
            av_log(nullptr, AV_LOG_INFO,
                   "audio encoder ready: %s, sample_fmt=%d, sample_rate=%d, channels=%d\n",
                   acodec->name, audio_enc_ctx->sample_fmt,
                   audio_enc_ctx->sample_rate, audio_enc_ctx->ch_layout.nb_channels);
        }
    }

    // ========== Open output file ==========
    if (avio_open(&fmt_ctx->pb, m_output_path.toUtf8().constData(), AVIO_FLAG_WRITE) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "open output file failed\n");
        return false;
    }

    if (avformat_write_header(fmt_ctx, nullptr) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "write header failed\n");
        return false;
    }

    yuv_frame = av_frame_alloc();
    yuv_frame->width = m_canvas_w;
    yuv_frame->height = m_canvas_h;
    yuv_frame->format = AV_PIX_FMT_YUV420P;
    if (av_frame_get_buffer(yuv_frame, 0) < 0) return false;

    pkt = av_packet_alloc();
    if (!pkt) return false;

    audio_frame_size = AUDIO_FRAME_SAMPLES;
    if (audio_enc_ctx && audio_enc_ctx->frame_size > 0)
        audio_frame_size = audio_enc_ctx->frame_size;

    if (has_audio) {
        audio_frame = av_frame_alloc();
        audio_frame->nb_samples = audio_frame_size;
        audio_frame->format = audio_enc_ctx->sample_fmt;
        audio_frame->sample_rate = AUDIO_SAMPLE_RATE;
        av_channel_layout_default(&audio_frame->ch_layout, AUDIO_CHANNELS);
        if (av_frame_get_buffer(audio_frame, 0) < 0) {
            av_frame_free(&audio_frame);
            audio_frame = nullptr;
        }
    }

    return true;
}

void Recoder::process_audio_source(DataSafeQueue<AVFramePtr> *src, SwrContext *swr,
                                   int64_t &silence_counter, std::deque<float> *fifo,
                                   bool &did_work) {
    while (!src->is_empty()) {
        auto frame = src->try_pop();
        if (!frame || !frame.value()) break;
        did_work = true;
        silence_counter = 0;

        if (init_audio_swr(swr, frame.value().get(), AUDIO_SAMPLE_RATE)) {
            AVFramePtr out(av_frame_alloc(), AVFrameDeleter());
            if (!out) continue;
            out->sample_rate = 48000;
            av_channel_layout_default(&out->ch_layout, 2);
            out->format = AV_SAMPLE_FMT_FLTP;
            out->nb_samples = av_rescale_rnd(
                swr_get_delay(swr, frame.value()->sample_rate) + frame.value()->nb_samples,
                48000, frame.value()->sample_rate, AV_ROUND_UP);
            if (av_frame_get_buffer(out.get(), 0) < 0) continue;

            int converted = swr_convert(swr, out->data, out->nb_samples,
                                        (const uint8_t **) frame.value()->data,
                                        frame.value()->nb_samples);
            if (converted > 0) {
                if (fifo[0].size() + converted > MAX_FIFO_SIZE) {
                    av_log(nullptr, AV_LOG_WARNING, "Audio FIFO overflow, dropping old samples\n");
                    size_t drop = fifo[0].size() + converted - MAX_FIFO_SIZE;
                    for (int ch = 0; ch < 2; ch++) {
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
    }
}

void Recoder::mix_audio(std::deque<float> *sys_fifo, std::deque<float> *mic_fifo,
                        std::deque<float> *audio_fifo, int64_t &audio_frames_received,
                        int64_t &audio_clock, bool &did_work) {
    size_t mix_length = 0;
    if (!sys_fifo[0].empty() && !mic_fifo[0].empty()) {
        mix_length = std::min(sys_fifo[0].size(), mic_fifo[0].size());
    } else if (!sys_fifo[0].empty()) {
        mix_length = sys_fifo[0].size();
        for (size_t i = 0; i < mix_length; i++) {
            mic_fifo[0].push_back(0.0f);
            mic_fifo[1].push_back(0.0f);
        }
    } else if (!mic_fifo[0].empty()) {
        mix_length = mic_fifo[0].size();
        for (size_t i = 0; i < mix_length; i++) {
            sys_fifo[0].push_back(0.0f);
            sys_fifo[1].push_back(0.0f);
        }
    }

    if (mix_length > 0) {
        did_work = true;
        for (size_t i = 0; i < mix_length; i++) {
            float mix_l = sys_fifo[0][i] + mic_fifo[0][i];
            float mix_r = sys_fifo[1][i] + mic_fifo[1][i];
            mix_l = tanhf(mix_l * 1.5f) / 1.5f;
            mix_r = tanhf(mix_r * 1.5f) / 1.5f;
            audio_fifo[0].push_back(mix_l);
            audio_fifo[1].push_back(mix_r);
        }

        for (int ch = 0; ch < 2; ch++) {
            sys_fifo[ch].erase(sys_fifo[ch].begin(), sys_fifo[ch].begin() + mix_length);
            mic_fifo[ch].erase(mic_fifo[ch].begin(), mic_fifo[ch].begin() + mix_length);
        }

        audio_frames_received += mix_length;
    }

    audio_clock += mix_length;
}

void Recoder::process_video_frame(SwsContext *&sws_ctx, AVFrame *yuv_frame,
                                  AVFormatContext *fmt_ctx, AVPacket *pkt,
                                  AVCodecContext *video_enc_ctx, AVStream *video_stream,
                                  int64_t &video_pts, bool has_audio,
                                  AVCodecContext *audio_enc_ctx, int64_t audio_clock,
                                  int &last_capture_w, int &last_capture_h,
                                  bool &did_work) {
    auto video_data = m_video_queue ? m_video_queue->try_pop() : std::nullopt;
    if (!video_data) return;
    did_work = true;

    int cap_w = video_data->width;
    int cap_h = video_data->height;

    if (!sws_ctx || cap_w != last_capture_w || cap_h != last_capture_h) {
        if (sws_ctx) sws_freeContext(sws_ctx);
        sws_ctx = sws_getContext(cap_w, cap_h, AV_PIX_FMT_BGRA,
                                 m_canvas_w, m_canvas_h, AV_PIX_FMT_YUV420P,
                                 SWS_BILINEAR, nullptr, nullptr, nullptr);
        last_capture_w = cap_w;
        last_capture_h = cap_h;
        if (!sws_ctx) {
            av_log(nullptr, AV_LOG_ERROR, "sws_getContext failed\n");
            return;
        }
    }

    uint8_t *src_slice[1] = {video_data->data.data()};
    int src_stride[1] = {video_data->stride};
    sws_scale(sws_ctx, src_slice, src_stride, 0, cap_h,
              yuv_frame->data, yuv_frame->linesize);

    if (has_audio && audio_enc_ctx) {
        video_pts = audio_clock * m_fps / AUDIO_SAMPLE_RATE;
    } else {
        video_pts++;
    }

    encode_video_frame(fmt_ctx, pkt, video_enc_ctx, video_stream, yuv_frame, video_pts);
}

void Recoder::encode_audio_frames(AVFrame *audio_frame, std::deque<float> *audio_fifo,
                                  int audio_frame_size, AVFormatContext *fmt_ctx, AVPacket *pkt,
                                  AVCodecContext *audio_enc_ctx, AVStream *audio_stream,
                                  int64_t &audio_pts, int64_t &audio_frames_encoded,
                                  bool &did_work) {
    if (!audio_frame) return;
    while ((int) audio_fifo[0].size() >= audio_frame_size) {
        did_work = true;
        int take = audio_frame_size;
        float *dst0 = (float *) audio_frame->data[0];
        float *dst1 = (float *) audio_frame->data[1];

        for (int j = 0; j < take; ++j) {
            dst0[j] = audio_fifo[0][j];
            dst1[j] = audio_fifo[1][j];
        }

        if (encode_audio_frame(fmt_ctx, pkt, audio_enc_ctx, audio_stream, audio_frame, audio_pts)) {
            audio_pts += take;
            audio_fifo[0].erase(audio_fifo[0].begin(), audio_fifo[0].begin() + take);
            audio_fifo[1].erase(audio_fifo[1].begin(), audio_fifo[1].begin() + take);
            audio_frames_encoded++;
        } else {
            break;
        }
    }
}

void Recoder::main_encode_loop(AVFormatContext *fmt_ctx, AVPacket *pkt,
                               AVCodecContext *video_enc_ctx, AVStream *video_stream,
                               AVCodecContext *audio_enc_ctx, AVStream *audio_stream,
                               SwsContext *&sws_ctx, SwrContext *sys_swr, SwrContext *mic_swr,
                               AVFrame *yuv_frame, AVFrame *audio_frame,
                               bool has_audio, int audio_frame_size,
                               std::deque<float> *audio_fifo,
                               std::deque<float> *sys_fifo,
                               std::deque<float> *mic_fifo,
                               int64_t &audio_clock, int64_t &video_pts, int64_t &audio_pts,
                               int64_t &audio_frames_received, int64_t &audio_frames_encoded,
                               int64_t &sys_silence_samples, int64_t &mic_silence_samples,
                               int &last_capture_w, int &last_capture_h) {
    while (m_recording.load()) {
        bool did_work = false;

        if (has_audio && m_system_audio_src) {
            process_audio_source(m_system_audio_src, sys_swr, sys_silence_samples, sys_fifo, did_work);
        }

        if (has_audio && m_mic_audio_src) {
            process_audio_source(m_mic_audio_src, mic_swr, mic_silence_samples, mic_fifo, did_work);
        }

        mix_audio(sys_fifo, mic_fifo, audio_fifo, audio_frames_received, audio_clock, did_work);

        process_video_frame(sws_ctx, yuv_frame, fmt_ctx, pkt, video_enc_ctx, video_stream,
                            video_pts, has_audio, audio_enc_ctx, audio_clock,
                            last_capture_w, last_capture_h, did_work);

        encode_audio_frames(audio_frame, audio_fifo, audio_frame_size, fmt_ctx, pkt,
                            audio_enc_ctx, audio_stream, audio_pts, audio_frames_encoded, did_work);

        static int iter_counter = 0;
        if (++iter_counter % 150 == 0) {
            av_log(nullptr, AV_LOG_INFO,
                   "encoding: audio_clock=%ld video_pts=%ld a_fifo=%zu sys=%zu mic=%zu a_recv=%d a_enc=%d\n",
                   audio_clock, video_pts,
                   audio_fifo[0].size(), sys_fifo[0].size(), mic_fifo[0].size(),
                   audio_frames_received, audio_frames_encoded);
        }

        if (!did_work) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void Recoder::drain_video_frames(SwsContext *&sws_ctx, AVFrame *yuv_frame,
                                 AVFormatContext *fmt_ctx, AVPacket *pkt,
                                 AVCodecContext *video_enc_ctx, AVStream *video_stream,
                                 int64_t &video_pts, int &last_capture_w, int &last_capture_h) {
    while (m_video_queue && !m_video_queue->is_empty()) {
        auto video_data = m_video_queue->try_pop();
        if (!video_data) break;

        int cap_w = video_data->width;
        int cap_h = video_data->height;
        if (!sws_ctx || cap_w != last_capture_w || cap_h != last_capture_h) {
            if (sws_ctx) sws_freeContext(sws_ctx);
            sws_ctx = sws_getContext(cap_w, cap_h, AV_PIX_FMT_BGRA,
                                     m_canvas_w, m_canvas_h, AV_PIX_FMT_YUV420P,
                                     SWS_BILINEAR, nullptr, nullptr, nullptr);
            last_capture_w = cap_w;
            last_capture_h = cap_h;
            if (!sws_ctx) break;
        }

        uint8_t *src_slice[1] = {video_data->data.data()};
        int src_stride[1] = {video_data->stride};
        sws_scale(sws_ctx, src_slice, src_stride, 0, cap_h,
                  yuv_frame->data, yuv_frame->linesize);

        encode_video_frame(fmt_ctx, pkt, video_enc_ctx, video_stream, yuv_frame, video_pts);
        video_pts++;
    }
}

void Recoder::flush_resamplers_and_mix_residual(SwrContext *sys_swr, SwrContext *mic_swr,
                                                std::deque<float> *sys_fifo, std::deque<float> *mic_fifo,
                                                std::deque<float> *audio_fifo, bool has_audio) {
    if (!has_audio) return;

    auto flush_swr = [&](SwrContext *swr, std::deque<float> *fifo) {
        if (!swr) return;
        AVFramePtr flush_frame(av_frame_alloc(), AVFrameDeleter());
        if (!flush_frame) return;
        flush_frame->sample_rate = AUDIO_SAMPLE_RATE;
        av_channel_layout_default(&flush_frame->ch_layout, AUDIO_CHANNELS);
        flush_frame->format = AUDIO_FORMAT;
        int delay_samples = swr_get_delay(swr, AUDIO_SAMPLE_RATE);
        if (delay_samples <= 0) return;
        flush_frame->nb_samples = delay_samples;
        if (av_frame_get_buffer(flush_frame.get(), 0) < 0) return;
        int converted = swr_convert(swr, flush_frame->data, flush_frame->nb_samples, nullptr, 0);
        if (converted > 0) {
            float *d0 = (float *) flush_frame->data[0];
            float *d1 = (float *) flush_frame->data[1];
            for (int i = 0; i < converted; ++i) {
                fifo[0].push_back(d0[i]);
                fifo[1].push_back(d1[i]);
            }
        }
    };

    flush_swr(sys_swr, sys_fifo);
    flush_swr(mic_swr, mic_fifo);

    while (!sys_fifo[0].empty() || !mic_fifo[0].empty()) {
        size_t max_len = std::max(sys_fifo[0].size(), mic_fifo[0].size());
        while (sys_fifo[0].size() < max_len) {
            sys_fifo[0].push_back(0.0f);
            sys_fifo[1].push_back(0.0f);
        }
        while (mic_fifo[0].size() < max_len) {
            mic_fifo[0].push_back(0.0f);
            mic_fifo[1].push_back(0.0f);
        }

        for (size_t i = 0; i < max_len; i++) {
            float mix_l = sys_fifo[0][i] + mic_fifo[0][i];
            float mix_r = sys_fifo[1][i] + mic_fifo[1][i];
            mix_l = tanhf(mix_l * 1.5f) / 1.5f;
            mix_r = tanhf(mix_r * 1.5f) / 1.5f;
            audio_fifo[0].push_back(mix_l);
            audio_fifo[1].push_back(mix_r);
        }

        sys_fifo[0].clear();
        sys_fifo[1].clear();
        mic_fifo[0].clear();
        mic_fifo[1].clear();
    }
}

void Recoder::drain_audio_fifo_residual(AVFrame *audio_frame, std::deque<float> *audio_fifo,
                                        int audio_frame_size, AVFormatContext *fmt_ctx,
                                        AVPacket *pkt, AVCodecContext *audio_enc_ctx,
                                        AVStream *audio_stream, int64_t &audio_pts,
                                        int64_t &audio_frames_encoded, bool has_audio) {
    if (!has_audio || !audio_frame || audio_fifo[0].empty()) return;

    while (!audio_fifo[0].empty()) {
        int remaining = (int) audio_fifo[0].size();
        int take = std::min(remaining, audio_frame_size);

        float *dst0 = (float *) audio_frame->data[0];
        float *dst1 = (float *) audio_frame->data[1];

        for (int j = 0; j < take; ++j) {
            dst0[j] = audio_fifo[0][j];
            dst1[j] = audio_fifo[1][j];
        }

        for (int j = take; j < audio_frame_size; ++j) {
            dst0[j] = 0.0f;
            dst1[j] = 0.0f;
        }

        if (encode_audio_frame(fmt_ctx, pkt, audio_enc_ctx, audio_stream, audio_frame, audio_pts)) {
            audio_pts += take;
            audio_fifo[0].erase(audio_fifo[0].begin(),
                                audio_fifo[0].begin() + std::min(take, (int) audio_fifo[0].size()));
            audio_fifo[1].erase(audio_fifo[1].begin(),
                                audio_fifo[1].begin() + std::min(take, (int) audio_fifo[1].size()));
            audio_frames_encoded++;
        }

        if (take < audio_frame_size) break;
    }
}

void Recoder::flush_encoders_and_trailer(AVFormatContext *fmt_ctx, AVPacket *pkt,
                                         AVCodecContext *video_enc_ctx, AVStream *video_stream,
                                         AVCodecContext *audio_enc_ctx, AVStream *audio_stream,
                                         bool has_audio) {
    avcodec_send_frame(video_enc_ctx, nullptr);
    while (avcodec_receive_packet(video_enc_ctx, pkt) == 0) {
        av_packet_rescale_ts(pkt, video_enc_ctx->time_base, video_stream->time_base);
        pkt->stream_index = video_stream->index;
        if (av_interleaved_write_frame(fmt_ctx, pkt) < 0) {
            av_log(nullptr, AV_LOG_ERROR, "video flush av_interleaved_write_frame failed\n");
        }
        av_packet_unref(pkt);
    }

    if (has_audio && audio_enc_ctx) {
        avcodec_send_frame(audio_enc_ctx, nullptr);
        while (avcodec_receive_packet(audio_enc_ctx, pkt) == 0) {
            av_packet_rescale_ts(pkt, audio_enc_ctx->time_base, audio_stream->time_base);
            pkt->stream_index = audio_stream->index;
            if (av_interleaved_write_frame(fmt_ctx, pkt) < 0) {
                av_log(nullptr, AV_LOG_ERROR, "audio flush av_interleaved_write_frame failed\n");
            }
            av_packet_unref(pkt);
        }
    }

    av_write_trailer(fmt_ctx);
}

void Recoder::drain_and_finalize(AVFormatContext *fmt_ctx, AVPacket *pkt,
                                 AVCodecContext *video_enc_ctx, AVStream *video_stream,
                                 AVCodecContext *audio_enc_ctx, AVStream *audio_stream,
                                 SwsContext *&sws_ctx, SwrContext *sys_swr, SwrContext *mic_swr,
                                 AVFrame *yuv_frame, AVFrame *audio_frame,
                                 bool has_audio, int audio_frame_size,
                                 std::deque<float> *audio_fifo,
                                 std::deque<float> *sys_fifo,
                                 std::deque<float> *mic_fifo,
                                 int64_t &video_pts, int64_t &audio_pts,
                                 int64_t &audio_frames_encoded,
                                 int &last_capture_w, int &last_capture_h) {
    drain_video_frames(sws_ctx, yuv_frame, fmt_ctx, pkt, video_enc_ctx, video_stream,
                       video_pts, last_capture_w, last_capture_h);

    flush_resamplers_and_mix_residual(sys_swr, mic_swr, sys_fifo, mic_fifo, audio_fifo, has_audio);

    drain_audio_fifo_residual(audio_frame, audio_fifo, audio_frame_size, fmt_ctx, pkt,
                              audio_enc_ctx, audio_stream, audio_pts, audio_frames_encoded, has_audio);

    flush_encoders_and_trailer(fmt_ctx, pkt, video_enc_ctx, video_stream,
                               audio_enc_ctx, audio_stream, has_audio);
}

void Recoder::encoding_loop() {
    AVFormatContext *fmt_ctx = nullptr;
    AVCodecContext *video_enc_ctx = nullptr;
    AVStream *video_stream = nullptr;
    AVCodecContext *audio_enc_ctx = nullptr;
    AVStream *audio_stream = nullptr;
    SwsContext *sws_ctx = nullptr;
    SwrContext *sys_swr = nullptr;
    SwrContext *mic_swr = nullptr;
    AVFrame *yuv_frame = nullptr;
    AVFrame *audio_frame = nullptr;
    AVPacket *pkt = nullptr;

    bool has_audio = m_system_audio_src || m_mic_audio_src;
    bool success = false;
    int last_capture_w = m_canvas_w;
    int last_capture_h = m_canvas_h;
    int audio_frame_size = AUDIO_FRAME_SAMPLES;
    int64_t audio_clock = 0, video_pts = 0, audio_pts = 0;
    int64_t audio_frames_received = 0, audio_frames_encoded = 0;
    int64_t sys_silence_samples = 0, mic_silence_samples = 0;
    std::deque<float> audio_fifo[2], sys_fifo[2], mic_fifo[2];

    if (setup_recording(fmt_ctx, video_enc_ctx, video_stream,
                        audio_enc_ctx, audio_stream,
                        has_audio, audio_frame_size,
                        yuv_frame, audio_frame, pkt)) {
        main_encode_loop(fmt_ctx, pkt, video_enc_ctx, video_stream,
                         audio_enc_ctx, audio_stream,
                         sws_ctx, sys_swr, mic_swr,
                         yuv_frame, audio_frame,
                         has_audio, audio_frame_size,
                         audio_fifo, sys_fifo, mic_fifo,
                         audio_clock, video_pts, audio_pts,
                         audio_frames_received, audio_frames_encoded,
                         sys_silence_samples, mic_silence_samples,
                         last_capture_w, last_capture_h);

        drain_and_finalize(fmt_ctx, pkt, video_enc_ctx, video_stream,
                           audio_enc_ctx, audio_stream,
                           sws_ctx, sys_swr, mic_swr,
                           yuv_frame, audio_frame,
                           has_audio, audio_frame_size,
                           audio_fifo, sys_fifo, mic_fifo,
                           video_pts, audio_pts,
                           audio_frames_encoded,
                           last_capture_w, last_capture_h);

        success = true;
    }

    if (!success)
        av_log(nullptr, AV_LOG_ERROR, "recording failed\n");

    cleanup_resources(fmt_ctx, pkt, yuv_frame, audio_frame,
                      sws_ctx, sys_swr, mic_swr,
                      video_enc_ctx, audio_enc_ctx);

    if (m_video_queue) m_video_queue->clean_queue();
}

void Recoder::cleanup_resources(AVFormatContext *&fmt_ctx, AVPacket *&pkt,
                                AVFrame *&yuv_frame, AVFrame *&audio_frame,
                                SwsContext *&sws_ctx, SwrContext *&sys_swr, SwrContext *&mic_swr,
                                AVCodecContext *&video_enc_ctx, AVCodecContext *&audio_enc_ctx) {
    av_packet_free(&pkt);
    av_frame_free(&yuv_frame);
    av_frame_free(&audio_frame);
    sws_freeContext(sws_ctx);
    if (sys_swr) swr_free(&sys_swr);
    if (mic_swr) swr_free(&mic_swr);
    if (video_enc_ctx) avcodec_free_context(&video_enc_ctx);
    if (audio_enc_ctx) avcodec_free_context(&audio_enc_ctx);
    if (fmt_ctx) {
        if (fmt_ctx->pb) avio_close(fmt_ctx->pb);
        avformat_free_context(fmt_ctx);
    }
}
