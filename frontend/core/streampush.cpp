#include "streampush.h"

void StreamPush::start(const QString &rtmp_url, int canvas_w, int canvas_h, int fps,
                       DataSafeQueue<AVFramePtr> *system_audio_src,
                       DataSafeQueue<AVFramePtr> *mic_audio_src) {
    m_rtmp_url = rtmp_url;
    Recoder::start(rtmp_url, canvas_w, canvas_h, fps, system_audio_src, mic_audio_src);
}

AVFormatOutputContextPtr StreamPush::create_format_context() {
    AVFormatContext *ctx = nullptr;
    int ret = avformat_alloc_output_context2(&ctx, nullptr, "flv",
                                             m_rtmp_url.toUtf8().constData());
    if (ret < 0 || !ctx) {
        av_log(nullptr, AV_LOG_ERROR, "streampush: alloc flv output context failed: %s\n",
               av_error_cxx(ret).c_str());
    }
    return AVFormatOutputContextPtr(ctx);
}

bool StreamPush::open_io(AVFormatOutputContextPtr &fmt_ctx) {
    QByteArray url_bytes = m_rtmp_url.toUtf8();
    const char *url = url_bytes.constData();
    av_log(nullptr, AV_LOG_INFO, "streampush: connecting to %s\n", url);
    int ret = avio_open(&fmt_ctx->pb, url, AVIO_FLAG_WRITE);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "streampush: avio_open failed: %s\n",
               av_error_cxx(ret).c_str());
    }
    return ret >= 0;
}