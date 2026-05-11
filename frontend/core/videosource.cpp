#include "videosource.h"

VideoSource::VideoSource(std::unique_ptr<VideoCaptor> captor)
    : m_capture(std::move(captor))
{
    if (m_capture) {
        m_capture->start();  // 统一启动采集
    }
}

void VideoSource::load_resources() {
    if (!m_texture_created) {
        create_texture(static_cast<int>(base_width), static_cast<int>(base_height));
    }
}

void VideoSource::unload_resources() {
    if (m_texture_created) {
        glDeleteTextures(1, &m_texture_id);
        m_texture_id = 0;
        m_texture_created = false;
    }
}

void VideoSource::render() {
    if (!m_texture_created || m_texture_id == 0) {
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

bool VideoSource::update_frame() {
    if (!m_capture || !m_capture->is_running()) return false;
    auto frame = m_capture->try_pop_frame();
    if (frame.has_value()) {
        upload_frame_to_texture(std::move(frame.value()));
        return true;
    }
    return false;
}

void VideoSource::set_frame_ready_callback(VideoCaptor::FrameReadyCallback callback) {
    if (m_capture) {
        m_capture->set_frame_ready_callback(std::move(callback));
    }
}

void VideoSource::create_texture(int width, int height) {
    glGenTextures(1, &m_texture_id);
    glBindTexture(GL_TEXTURE_2D, m_texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
    m_tex_width = width;
    m_tex_height = height;
    m_texture_created = true;
}

void VideoSource::upload_frame_to_texture(const AVFramePtr& frame) {
    if (!frame) return;
    int fw = frame->width;
    int fh = frame->height;
    if (fw != m_tex_width || fh != m_tex_height) {
        glDeleteTextures(1, &m_texture_id);
        create_texture(fw, fh);
    }
    glBindTexture(GL_TEXTURE_2D, m_texture_id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fw, fh,
                    GL_BGRA, GL_UNSIGNED_BYTE, frame->data[0]);
}