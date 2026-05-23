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

    do {
        avformat_alloc_output_context2(&fmt_ctx, nullptr, "mp4",
                                       m_output_path.toUtf8().constData());
        if (!fmt_ctx) {
            av_log(nullptr, AV_LOG_ERROR, "alloc output context failed\n");
            break;
        }

        // ========== Video encoder ==========
        const AVCodec *vcodec = avcodec_find_encoder_by_name("libx264");
        if (!vcodec) vcodec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
        if (!vcodec) {
            av_log(nullptr, AV_LOG_ERROR, "no video encoder\n");
            break;
        }

        video_enc_ctx = avcodec_alloc_context3(vcodec);
        if (!video_enc_ctx) break;
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
                break;
            }
        }

        video_stream = avformat_new_stream(fmt_ctx, nullptr);
        if (!video_stream) break;
        video_stream->time_base = video_enc_ctx->time_base;
        video_stream->id = 0;
        avcodec_parameters_from_context(video_stream->codecpar, video_enc_ctx);

        // ========== Audio encoder (try multiple codecs) ==========
        if (has_audio) {
            static const AVCodecID audio_codec_candidates[] = {
                AV_CODEC_ID_AAC,
                AV_CODEC_ID_MP3,
                AV_CODEC_ID_OPUS,
                AV_CODEC_ID_NONE
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
                has_audio = false;
            } else {
                audio_enc_ctx = avcodec_alloc_context3(acodec);
                if (!audio_enc_ctx) break;
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
                if (!audio_stream) break;
                audio_stream->time_base = audio_enc_ctx->time_base;
                audio_stream->id = 1;
                avcodec_parameters_from_context(audio_stream->codecpar, audio_enc_ctx);
                av_log(nullptr, AV_LOG_INFO,
                       "audio encoder ready: %s, sample_fmt=%d, sample_rate=%d, channels=%d\n",
                       acodec->name, audio_enc_ctx->sample_fmt,
                       audio_enc_ctx->sample_rate, audio_enc_ctx->ch_layout.nb_channels);
            } else {
                av_log(nullptr, AV_LOG_WARNING, "audio encoder init failed, recording without audio\n");
                has_audio = false;
            }
        }

        // ========== Open output file ==========
        if (fmt_ctx->pb) {
            avio_close(fmt_ctx->pb);
            fmt_ctx->pb = nullptr;
        }
        if (avio_open(&fmt_ctx->pb, m_output_path.toUtf8().constData(), AVIO_FLAG_WRITE) < 0) {
            av_log(nullptr, AV_LOG_ERROR, "open output file failed\n");
            break;
        }

        if (avformat_write_header(fmt_ctx, nullptr) < 0) {
            av_log(nullptr, AV_LOG_ERROR, "write header failed\n");
            break;
        }

        // sws_ctx will be (re)created on first frame with actual capture dimensions
        sws_ctx = nullptr;

        yuv_frame = av_frame_alloc();
        yuv_frame->width = m_canvas_w;
        yuv_frame->height = m_canvas_h;
        yuv_frame->format = AV_PIX_FMT_YUV420P;
        if (av_frame_get_buffer(yuv_frame, 0) < 0) break;

        pkt = av_packet_alloc();
        if (!pkt) break;

        int audio_frame_size = AUDIO_FRAME_SAMPLES;
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

        // Single mixed audio FIFO (stereo planar float @ 48kHz)
        std::deque<float> audio_fifo[2];

        int64_t video_pts = 0;
        int64_t audio_pts = 0;
        int audio_frames_received = 0;
        int audio_frames_encoded = 0;

        // ========== Main encoding loop ==========
        while (m_recording.load()) {
            bool did_work = false;

            // --- Process video (audio-driven sync) ---
            auto video_data = m_video_queue ? m_video_queue->try_pop() : std::nullopt;
            if (video_data) {
                int cap_w = video_data->width;
                int cap_h = video_data->height;

                // Recreate sws_ctx if capture dimensions changed
                if (!sws_ctx || cap_w != last_capture_w || cap_h != last_capture_h) {
                    if (sws_ctx) sws_freeContext(sws_ctx);
                    sws_ctx = sws_getContext(cap_w, cap_h, AV_PIX_FMT_BGRA,
                                             m_canvas_w, m_canvas_h, AV_PIX_FMT_YUV420P,
                                             SWS_BILINEAR, nullptr, nullptr, nullptr);
                    last_capture_w = cap_w;
                    last_capture_h = cap_h;
                    if (!sws_ctx) {
                        av_log(nullptr, AV_LOG_ERROR, "sws_getContext failed\n");
                        continue;
                    }
                }

                // Audio-driven sync: only encode video when audio has advanced enough
                bool should_encode = true;
                if (has_audio) {
                    int64_t audio_pts_scaled = audio_pts * m_fps / AUDIO_SAMPLE_RATE;
                    should_encode = (audio_pts_scaled > video_pts);
                }

                if (should_encode) {
                    did_work = true;
                    uint8_t *src_slice[1] = {video_data->data.data()};
                    int src_stride[1] = {video_data->stride};
                    sws_scale(sws_ctx, src_slice, src_stride, 0, cap_h,
                              yuv_frame->data, yuv_frame->linesize);

                    if (has_audio) {
                        video_pts = audio_pts * m_fps / AUDIO_SAMPLE_RATE;
                    }
                    encode_video_frame(fmt_ctx, pkt, video_enc_ctx, video_stream, yuv_frame, video_pts);
                    video_pts++;
                }
            }

            // --- Drain ALL system audio frames, resample & mix into FIFO ---
            if (has_audio && m_system_audio_src) {
                while (!m_system_audio_src->is_empty()) {
                    auto sys_frame = m_system_audio_src->try_pop();
                    if (!sys_frame || !sys_frame.value()) break;
                    did_work = true;
                    audio_frames_received++;
                    if (init_audio_swr(sys_swr, sys_frame.value().get(), AUDIO_SAMPLE_RATE)) {
                        AVFramePtr out(av_frame_alloc(), AVFrameDeleter());
                        if (!out) continue;
                        out->sample_rate = 48000;
                        av_channel_layout_default(&out->ch_layout, 2);
                        out->format = AV_SAMPLE_FMT_FLTP;
                        out->nb_samples = av_rescale_rnd(
                            swr_get_delay(sys_swr, sys_frame.value()->sample_rate) + sys_frame.value()->nb_samples,
                            48000, sys_frame.value()->sample_rate, AV_ROUND_UP);
                        if (av_frame_get_buffer(out.get(), 0) < 0) continue;
                        int converted = swr_convert(sys_swr, out->data, out->nb_samples,
                                                    (const uint8_t **) sys_frame.value()->data,
                                                    sys_frame.value()->nb_samples);
                        if (converted > 0) {
                            float *d0 = (float *) out->data[0];
                            float *d1 = (float *) out->data[1];
                            for (int i = 0; i < converted; ++i) {
                                float s = d0[i] * 0.7f;
                                audio_fifo[0].push_back(s > 1.0f ? 1.0f : (s < -1.0f ? -1.0f : s));
                                s = d1[i] * 0.7f;
                                audio_fifo[1].push_back(s > 1.0f ? 1.0f : (s < -1.0f ? -1.0f : s));
                            }
                        }
                    }
                }
            }

            // --- Drain ALL mic audio frames, resample & mix into FIFO ---
            // if (has_audio && m_mic_audio_src) {
            //     while (!m_mic_audio_src->is_empty()) {
            //         auto mic_frame = m_mic_audio_src->try_pop();
            //         if (!mic_frame || !mic_frame.value()) break;
            //         did_work = true;
            //         audio_frames_received++;
            //         if (init_audio_swr(mic_swr, mic_frame.value().get(), AUDIO_SAMPLE_RATE)) {
            //             AVFramePtr out(av_frame_alloc(), AVFrameDeleter());
            //             if (!out) continue;
            //             out->sample_rate = 48000;
            //             av_channel_layout_default(&out->ch_layout, 2);
            //             out->format = AV_SAMPLE_FMT_FLTP;
            //             out->nb_samples = av_rescale_rnd(
            //                 swr_get_delay(mic_swr, mic_frame.value()->sample_rate) + mic_frame.value()->nb_samples,
            //                 48000, mic_frame.value()->sample_rate, AV_ROUND_UP);
            //             if (av_frame_get_buffer(out.get(), 0) < 0) continue;
            //             int converted = swr_convert(mic_swr, out->data, out->nb_samples,
            //                                         (const uint8_t **) mic_frame.value()->data,
            //                                         mic_frame.value()->nb_samples);
            //             if (converted > 0) {
            //                 float *d0 = (float *) out->data[0];
            //                 float *d1 = (float *) out->data[1];
            //                 for (int i = 0; i < converted; ++i) {
            //                     float s = d0[i] * 0.7f;
            //                     audio_fifo[0].push_back(s > 1.0f ? 1.0f : (s < -1.0f ? -1.0f : s));
            //                     s = d1[i] * 0.7f;
            //                     audio_fifo[1].push_back(s > 1.0f ? 1.0f : (s < -1.0f ? -1.0f : s));
            //                 }
            //             }
            //         }
            //     }
            // }

            // --- Encode as many audio frames as possible from FIFO ---
            if (has_audio && audio_frame) {
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
                        break; // encoder rejected the frame, stop encoding
                    }
                }
            }

            static int iter_counter = 0;
            if (++iter_counter % 150 == 0) {
                av_log(nullptr, AV_LOG_INFO,
                       "encoding: video_pts=%ld audio_pts=%ld a_recv=%d a_enc=%d fifo=%zu\n",
                       video_pts, audio_pts, audio_frames_received, audio_frames_encoded,
                       audio_fifo[0].size());
            }

            if (!did_work) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        // ========== Drain remaining video frames ==========
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

        // ========== Flush video ==========
        avcodec_send_frame(video_enc_ctx, nullptr);
        while (avcodec_receive_packet(video_enc_ctx, pkt) == 0) {
            av_packet_rescale_ts(pkt, video_enc_ctx->time_base, video_stream->time_base);
            pkt->stream_index = video_stream->index;
            if (av_interleaved_write_frame(fmt_ctx, pkt) < 0) {
                av_log(nullptr, AV_LOG_ERROR, "video flush av_interleaved_write_frame failed\n");
            }
            av_packet_unref(pkt);
        }

        // ========== Flush SWR residual samples ==========
        if (has_audio) {
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
            flush_swr(sys_swr, audio_fifo);
            flush_swr(mic_swr, audio_fifo);
        }

        // ========== Drain remaining FIFO audio after SWR flush ==========
        if (has_audio && audio_frame && (int) audio_fifo[0].size() > 0) {
            int remaining = (int) audio_fifo[0].size();
            int take = audio_frame_size;
            while (remaining >= take) {
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
                }
                if (audio_fifo[0].empty()) break;
                remaining = (int) audio_fifo[0].size();
                take = std::min(take, remaining);
            }
            // Log any lost samples
            if (!audio_fifo[0].empty()) {
                av_log(nullptr, AV_LOG_WARNING, "audio FIFO has %zu leftover samples (discarded)\n",
                       audio_fifo[0].size());
            }
        }

        // ========== Flush audio encoder (drain internal encoder buffers only) ==========
        if (has_audio) {
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
        success = true;
    } while (false);

    if (!success)
        av_log(nullptr, AV_LOG_ERROR, "recording failed\n");

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

    if (m_video_queue) m_video_queue->clean_queue();
}
