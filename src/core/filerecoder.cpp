#include "filerecoder.h"

void FileRecoder::start(const std::string &output_path, int canvas_w, int canvas_h, int fps,
                        DataSafeQueue<AVFramePtr> *system_audio_src,
                        DataSafeQueue<AVFramePtr> *mic_audio_src) {
    m_output_path = output_path;
    Recoder::start(output_path, canvas_w, canvas_h, fps, system_audio_src, mic_audio_src);
}

AVFormatOutputContextPtr FileRecoder::create_format_context() {
    // Allocate MP4 muxer context
    AVFormatContext *ctx = nullptr;
    avformat_alloc_output_context2(&ctx, nullptr, "mp4",
                                   m_output_path.c_str());
    return AVFormatOutputContextPtr(ctx);
}

bool FileRecoder::open_io(AVFormatOutputContextPtr &fmt_ctx) {
    return avio_open(&fmt_ctx->pb,
                     m_output_path.c_str(),
                     AVIO_FLAG_WRITE) >= 0;
}
