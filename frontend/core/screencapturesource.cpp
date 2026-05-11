//
// Created by 66 on 2026/5/8.
//

#include "screencapturesource.h"

ScreenCaptureSource::ScreenCaptureSource(int screen_index) {
    // ===== 1. 几何属性 =====
    base_width  = 1920.0f;
    base_height = 1080.0f;
    pos_x = 0.0f;
    pos_y = 0.0f;
    scale_x = 1.0f;
    scale_y = 1.0f;
    visible = true;
    lock_aspect_ratio = true;
    fixed_aspect_ratio = 16.0f / 9.0f;

    // ===== 2. 创建采集类并启动采集线程 =====
    m_capture = std::make_unique<ScreenCaptor>();
    m_capture->start();   // 不阻塞，内部 detach 线程
}

ScreenCaptureSource::~ScreenCaptureSource() {
    if (m_capture) {
        m_capture->stop();
    }
}

// ========== Source 虚函数实现 ==========

void ScreenCaptureSource::load_resources() {
    // OpenGL 上下文已就绪，创建纹理（用默认分辨率）
    if (!m_texture_created) {
        create_texture(static_cast<int>(base_width), static_cast<int>(base_height));
    }
}

void ScreenCaptureSource::unload_resources() {
    if (m_texture_created) {
        glDeleteTextures(1, &m_texture_id);
        m_texture_id = 0;
        m_texture_created = false;
    }
}

void ScreenCaptureSource::render() {
    if (!m_texture_created || m_texture_id == 0) {
        // 降级：灰色占位矩形
        glColor4f(0.2f, 0.2f, 0.2f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(0.0f, 0.0f);
        glVertex2f(base_width, 0.0f);
        glVertex2f(base_width, base_height);
        glVertex2f(0.0f, base_height);
        glEnd();
        return;
    }

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_texture_id);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(base_width, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(base_width, base_height);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, base_height);
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

// ========== 纹理操作 ==========

void ScreenCaptureSource::create_texture(int width, int height) {
    glGenTextures(1, &m_texture_id);
    glBindTexture(GL_TEXTURE_2D, m_texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    m_tex_width = width;
    m_tex_height = height;
    m_texture_created = true;
}

void ScreenCaptureSource::upload_frame_to_texture(const AVFramePtr& frame) {
    if (!frame) return;

    int fw = frame->width;
    int fh = frame->height;

    if (fw != m_tex_width || fh != m_tex_height) {
        glDeleteTextures(1, &m_texture_id);
        create_texture(fw, fh);
        // ⚠️ create_texture 内部已经 bind 了新纹理，但之后没有显式 bind
    }

    glBindTexture(GL_TEXTURE_2D, m_texture_id);  // ✅ 这个 bind 其实已经有了，保留即可

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fw, fh,
                    GL_BGRA, GL_UNSIGNED_BYTE, frame->data[0]);
}

// ========== 主线程每帧调用 ==========
bool ScreenCaptureSource::update_texture_if_new_frame() {
    if (!m_capture || !m_capture->is_running()) return false;

    auto frame = m_capture->try_pop_frame();  // 返回值 move
    if (frame.has_value()) {
        upload_frame_to_texture(std::move(frame.value()));
        return true;
    }
    return false;
}

void ScreenCaptureSource::set_frame_ready_callback(ScreenCaptor::FrameReadyCallback callback) {
    if (m_capture) {
        m_capture->set_frame_ready_callback(std::move(callback));
    }
}