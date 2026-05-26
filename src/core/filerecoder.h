#ifndef OBSIM_FILERECODER_H
#define OBSIM_FILERECODER_H

#include "base/recoder.h"

class FileRecoder : public Recoder {
public:
    FileRecoder() = default;

    void start(const std::string &output_path, int canvas_w, int canvas_h, int fps = 30,
               DataSafeQueue<AVFramePtr> *system_audio_src = nullptr,
               DataSafeQueue<AVFramePtr> *mic_audio_src = nullptr) override;

protected:
    AVFormatOutputContextPtr create_format_context() override;
    bool open_io(AVFormatOutputContextPtr &fmt_ctx) override;

private:
    std::string m_output_path;
};

#endif