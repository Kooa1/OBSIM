#ifndef OBSIM_FILERECODER_H
#define OBSIM_FILERECODER_H

#include "base/recoder.h"

/// @brief File-based recording implementation that writes MP4 output to disk.
class FileRecoder : public Recoder {
public:
    FileRecoder() = default;

    /// @brief Starts recording to the specified output file path.
    void start(const std::string &output_path, int canvas_w, int canvas_h, int fps = 30,
               DataSafeQueue<AVFramePtr> *system_audio_src = nullptr,
               DataSafeQueue<AVFramePtr> *mic_audio_src = nullptr) override;

protected:
    AVFormatOutputContextPtr create_format_context() override;
    bool open_io(AVFormatOutputContextPtr &fmt_ctx) override;

private:
    std::string m_output_path; ///< Output file path for the recording
};

#endif