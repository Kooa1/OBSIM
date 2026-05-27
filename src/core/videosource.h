#ifndef OBSIM_VIDEOSOURCE_H
#define OBSIM_VIDEOSOURCE_H

#include <QOpenGLFunctions>

#include <GL/gl.h>

#include "base/source.h"
#include "base/videocaptor.h"

class OpenCVFilter;

class VideoSource : public Source {
public:
    explicit VideoSource(std::unique_ptr<VideoCaptor> captor);
    ~VideoSource() override = default;

    void load_resources() override;
    void unload_resources() override;
    void render() override;
    bool update_frame() override;
    void set_frame_ready_callback(VideoCaptor::FrameReadyCallback callback);
    void stop_capture();
    void pause_capture();
    void resume_capture();
    OpenCVFilter* filter();
    bool update_filtered_frame();
    void start_filter();
    void stop_filter();

protected:
    void create_texture(int width, int height);
    void upload_frame_to_texture(const AVFramePtr& frame);

    std::unique_ptr<VideoCaptor> m_capture;
    GLuint m_texture_id = 0;
    int    m_tex_width = 0;
    int    m_tex_height = 0;
    bool   m_texture_created = false;
};

#endif