//
// Created by 66 on 2026/5/8.
//

#ifndef OBSIM_SCREENCAPTURESOURCE_H
#define OBSIM_SCREENCAPTURESOURCE_H

#include "core/source.h"
#include "core/screencaptor.h"

class ScreenCaptureSource : public Source {
public:
    // 构造函数接收采集参数（屏幕索引、设备名等）
    explicit ScreenCaptureSource(int screen_index);
    ~ScreenCaptureSource() override;

    // ========== Source 虚函数 ==========
    void render() override;
    const char* source_type_name() const override { return "Screen Capture"; }
    void load_resources() override;      // OpenGL 上下文就绪时创建纹理
    void unload_resources() override;    // 释放纹理

    // ========== 给 Widget 调用的接口 ==========
    bool try_get_latest_frame(FrameData& out_frame);
    void update_texture(const FrameData& frame);
    bool is_capturing() const;

private:
    void create_texture(int width, int height);

private:
    // ✅ 持有你的采集类实例
    std::unique_ptr<ScreenCaptor> m_capture;

    // OpenGL 纹理
    GLuint m_texture_id = 0;
    int    m_tex_width = 0;
    int    m_tex_height = 0;
    bool   m_texture_created = false;
};


#endif //OBSIM_SCREENCAPTURESOURCE_H
