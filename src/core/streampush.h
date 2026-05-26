#ifndef OBSIM_STREAMPUSH_H
#define OBSIM_STREAMPUSH_H

#include "base/recoder.h"
#include "../utils/av_err2str_cxx.h"

extern "C" {
#include <libavutil/log.h>
}

class StreamPush : public Recoder {
public:
    StreamPush() = default;

    void start(const std::string &rtmp_url, int canvas_w, int canvas_h, int fps = 30,
               DataSafeQueue<AVFramePtr> *system_audio_src = nullptr,
               DataSafeQueue<AVFramePtr> *mic_audio_src = nullptr) override;

protected:
    AVFormatOutputContextPtr create_format_context() override;

    bool open_io(AVFormatOutputContextPtr &fmt_ctx) override;

private:
    std::string m_rtmp_url;
};

#endif
