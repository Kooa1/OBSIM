//
// Created by 66 on 2026/5/8.
//

#ifndef OBSIM_SCREENCAPTURESOURCE_H
#define OBSIM_SCREENCAPTURESOURCE_H

#include "core/source.h"
#include "core/screencaptor.h"

class ScreenCaptureSource : public Source {
public:
    explicit ScreenCaptureSource(int screen_index);
    ~ScreenCaptureSource() override;

    // ========== Source 虚函数 ==========
    void render() override;
    const char* source_type_name() const override { return "Screen Capture"; }
    void load_resources() override;
    void unload_resources() override;

    // ========== 主线程每帧调用 ==========
    bool update_texture_if_new_frame();

    void set_frame_ready_callback(ScreenCaptor::FrameReadyCallback callback);

private:
    void create_texture(int width, int height);
    void upload_frame_to_texture(const AVFramePtr& frame);

    std::unique_ptr<ScreenCaptor> m_capture;
    GLuint m_texture_id = 0;
    int    m_tex_width = 0;
    int    m_tex_height = 0;
    bool   m_texture_created = false;

};


#endif //OBSIM_SCREENCAPTURESOURCE_H
