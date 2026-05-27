#include "textsource.h"

TextSource::TextSource()
    : m_text("Text")
      , m_font("Arial", 48)
      , m_color(255, 255, 255) {
    QSize size = calculate_text_size();
    base_width = static_cast<float>(size.width());
    base_height = static_cast<float>(size.height());
}

TextSource::~TextSource() = default;

void TextSource::load_resources() {
    rebuild_texture();
}

void TextSource::unload_resources() {
    if (!m_gl_initialized) {
        initializeOpenGLFunctions();
        m_gl_initialized = true;
    }
    if (m_texture_id) {
        this->glDeleteTextures(1, &m_texture_id);
        m_texture_id = 0;
        m_tex_width = 0;
        m_tex_height = 0;
    }
}

void TextSource::render() {
    if (m_dirty) {
        rebuild_texture();
    }

    if (!m_texture_id) {
        glColor4f(0.3f, 0.3f, 0.3f, 1.0f);
        glBegin(GL_QUADS);
        glVertex2f(0.0f, 0.0f);
        glVertex2f(base_width, 0.0f);
        glVertex2f(base_width, base_height);
        glVertex2f(0.0f, base_height);
        glEnd();
        return;
    }

    this->glEnable(GL_TEXTURE_2D);
    this->glBindTexture(GL_TEXTURE_2D, m_texture_id);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    this->glEnable(GL_BLEND);
    this->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(base_width, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(base_width, base_height);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(0.0f, base_height);
    glEnd();

    this->glDisable(GL_BLEND);
    this->glDisable(GL_TEXTURE_2D);
}

void TextSource::set_text(const QString &text) {
    if (m_text != text) {
        m_text = text;
        QSize size = calculate_text_size();
        base_width = static_cast<float>(size.width());
        base_height = static_cast<float>(size.height());
        m_dirty = true;
    }
}

void TextSource::set_font(const QFont &font) {
    m_font = font;
    QSize size = calculate_text_size();
    base_width = static_cast<float>(size.width());
    base_height = static_cast<float>(size.height());
    m_dirty = true;
}

void TextSource::set_color(const QColor &color) {
    m_color = color;
    m_dirty = true;
}

void TextSource::rebuild_texture() {
    m_dirty = false;

    if (!m_gl_initialized) {
        initializeOpenGLFunctions();
        m_gl_initialized = true;
    }

    if (m_text.isEmpty()) return;

    if (m_texture_id) {
        this->glDeleteTextures(1, &m_texture_id);
        m_texture_id = 0;
    }

    QSize size = calculate_text_size();
    int w = size.width();
    int h = size.height();
    if (w <= 0 || h <= 0) return;

    QImage image(w, h, QImage::Format_ARGB32);
    image.fill(Qt::transparent); {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setFont(m_font);
        painter.setPen(m_color);
        painter.drawText(QRect(0, 0, w, h), Qt::AlignLeft | Qt::AlignTop, m_text);
    }

    m_tex_width = w;
    m_tex_height = h;

    this->glGenTextures(1, &m_texture_id);
    this->glBindTexture(GL_TEXTURE_2D, m_texture_id);
    this->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    this->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    this->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    this->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    this->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_tex_width, m_tex_height, 0,
                       GL_BGRA, GL_UNSIGNED_BYTE, image.constBits());
}

QSize TextSource::calculate_text_size() const {
    if (m_text.isEmpty()) return QSize(1, 1);
    QFontMetrics fm(m_font);
    return QSize(fm.horizontalAdvance(m_text) + 8, fm.height() + 8);
}
