#ifndef OBSIM_TEXTSOURCE_H
#define OBSIM_TEXTSOURCE_H

#include <QOpenGLFunctions>
#include <QString>
#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QFontMetrics>

#include <GL/gl.h>

#include "base/source.h"

/// @brief Text source that renders text as an OpenGL textured quad
class TextSource : public Source, protected QOpenGLFunctions {
public:
    TextSource();

    ~TextSource() override;

    /// @brief Load OpenGL resources (create texture from text)
    void load_resources() override;

    /// @brief Unload OpenGL resources (delete texture)
    void unload_resources() override;

    /// @brief Render the text as an OpenGL textured quad
    void render() override;

    const char *source_type_name() const override { return "Text"; }

    /// @brief Set the text content to display
    /// @param text Text string to render
    void set_text(const QString &text);
    /// @brief Set the font for text rendering
    /// @param font Font to use
    void set_font(const QFont &font);
    /// @brief Set the text color
    /// @param color Color for text
    void set_color(const QColor &color);

    QString text() const { return m_text; }
    QFont font() const { return m_font; }
    QColor color() const { return m_color; }

private:
    /// @brief Rebuild the OpenGL texture from current text content
    void rebuild_texture();
    /// @brief Calculate the required text render size
    /// @return Size that fits the text with padding
    QSize calculate_text_size() const;

private:

    QString m_text;               ///< Text content to display
    QFont m_font;                 ///< Font for text rendering
    QColor m_color;               ///< Color of the text
    GLuint m_texture_id = 0;      ///< OpenGL texture ID
    int m_tex_width = 0;          ///< Texture width in pixels
    int m_tex_height = 0;         ///< Texture height in pixels
    bool m_dirty = true;          ///< Whether the texture needs to be rebuilt
    bool m_gl_initialized = false; ///< Whether OpenGL functions have been initialized
};

#endif