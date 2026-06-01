#include "fileoutput.h"



FileOutput::FileOutput()
    : m_packet_queue(std::make_unique<DataSafeQueue<AVPacketPtr>>(300)) {
}

FileOutput::~FileOutput() {
    stop();
}

bool FileOutput::start(const std::string &output_path,
                        AVCodecParameters *video_codecpar,
                        AVCodecParameters *audio_codecpar,
                        AVRational video_time_base,
                        AVRational audio_time_base) {
    stop();
    m_output_path = output_path;
    m_has_audio = audio_codecpar != nullptr;
    m_video_time_base = video_time_base;
    m_audio_time_base = audio_time_base;

    AVFormatContext *ctx = nullptr;
    int ret = avformat_alloc_output_context2(&ctx, nullptr, "mp4", output_path.c_str());
    if (ret < 0 || !ctx) {
        av_log(nullptr, AV_LOG_ERROR, "fileoutput: alloc mp4 context failed: %s\n",
               av_error_cxx(ret).c_str());
        return false;
    }
    m_fmt_ctx = AVFormatOutputContextPtr(ctx);

    AVStream *vstream = avformat_new_stream(m_fmt_ctx.get(), nullptr);
    if (!vstream) {
        av_log(nullptr, AV_LOG_ERROR, "fileoutput: add video stream failed\n");
        return false;
    }
    avcodec_parameters_copy(vstream->codecpar, video_codecpar);
    vstream->time_base = video_time_base;
    m_video_stream_index = vstream->index;

    if (m_has_audio) {
        AVStream *astream = avformat_new_stream(m_fmt_ctx.get(), nullptr);
        if (!astream) {
            av_log(nullptr, AV_LOG_ERROR, "fileoutput: add audio stream failed\n");
            return false;
        }
        avcodec_parameters_copy(astream->codecpar, audio_codecpar);
        astream->time_base = audio_time_base;
        m_audio_stream_index = astream->index;
    }

    if (avio_open(&m_fmt_ctx->pb, output_path.c_str(), AVIO_FLAG_WRITE) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "fileoutput: avio_open failed: %s\n", output_path.c_str());
        return false;
    }

    if (avformat_write_header(m_fmt_ctx.get(), nullptr) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "fileoutput: write header failed\n");
        return false;
    }

    m_flush_done.store(false);
    m_running.store(true);
    m_write_thread = std::thread([this]() { write_loop(); });

    av_log(nullptr, AV_LOG_INFO, "fileoutput: started: %s\n", output_path.c_str());
    return true;
}

void FileOutput::stop() {
    if (!m_running.load()) return;
    m_running.store(false);

    if (m_write_thread.joinable()) {
        m_packet_queue->stop_work();
        m_write_thread.join();
    }

    if (m_fmt_ctx) {
        av_write_trailer(m_fmt_ctx.get());
    }

    m_fmt_ctx.reset();
    m_packet_queue->clean_queue();
    m_flush_done.store(false);
    m_video_stream_index = -1;
    m_audio_stream_index = -1;

    av_log(nullptr, AV_LOG_INFO, "fileoutput: stopped: %s\n", m_output_path.c_str());
}

void FileOutput::feed_packet(AVPacketPtr pkt) {
    if (!m_running.load() || !m_packet_queue) return;
    m_packet_queue->push_no_wait(std::move(pkt));
}

bool FileOutput::is_running() const {
    return m_running.load();
}

void FileOutput::write_loop() {
    while (true) {
        auto pkt_opt = m_packet_queue->try_pop();
        if (pkt_opt) {
            auto *pkt = pkt_opt->get();
            if (pkt->stream_index == m_video_stream_index) {
                av_packet_rescale_ts(pkt, m_video_time_base,
                                     m_fmt_ctx->streams[m_video_stream_index]->time_base);
            } else if (pkt->stream_index == m_audio_stream_index) {
                av_packet_rescale_ts(pkt, m_audio_time_base,
                                     m_fmt_ctx->streams[m_audio_stream_index]->time_base);
            }
            if (av_interleaved_write_frame(m_fmt_ctx.get(), pkt) < 0) {
                av_log(nullptr, AV_LOG_ERROR, "fileoutput: write frame failed\n");
            }
        } else if (!m_running.load() && m_packet_queue->is_empty()) {
            break;
        }
    }
}
