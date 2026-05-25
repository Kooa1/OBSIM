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

#include "../base/source.h"

class TextSource : public Source, protected QOpenGLFunctions {
public:
    TextSource();

    ~TextSource() override;

    void load_resources() override;

    void unload_resources() override;

    void render() override;

    const char *source_type_name() const override { return "Text"; }

    void set_text(const QString &text);

    void set_font(const QFont &font);

    void set_color(const QColor &color);

private:
    void rebuild_texture();

    QSize calculate_text_size() const;

private:

    QString m_text;
    QFont m_font;
    QColor m_color;
    GLuint m_texture_id = 0;
    int m_tex_width = 0;
    int m_tex_height = 0;
    bool m_dirty = true;
    bool m_gl_initialized = false;
};

#endif
