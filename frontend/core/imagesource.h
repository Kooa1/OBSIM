#ifndef OBSIM_IMAGESOURCE_H
#define OBSIM_IMAGESOURCE_H

#include <QOpenGLFunctions>
#include <QString>
#include <QImage>
#include <QFile>

extern "C" {
#include <libavutil/imgutils.h>
};

#include "source.h"
#include "../utils/ffmpegfactory.h"


class ImageSource : public Source, protected QOpenGLFunctions {
public:
    explicit ImageSource(const QString &file_path);

    ~ImageSource() override;

    void load_resources() override;

    void unload_resources() override;

    void render() override;

    const char *source_type_name() const override { return "Image"; }

    const QString &file_path() const { return m_file_path; }

private:
    bool load_image();
    void ensure_texture();

private:
    QString m_file_path;
    GLuint m_texture_id = 0;
    int m_tex_width = 0;
    int m_tex_height = 0;
    bool m_gl_initialized = false;
};

#endif
