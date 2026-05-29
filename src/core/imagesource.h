#ifndef OBSIM_IMAGESOURCE_H
#define OBSIM_IMAGESOURCE_H

#include <QOpenGLFunctions>
#include <QString>
#include <QImage>
#include <QFile>

extern "C" {
#include <libavutil/imgutils.h>
};

#include "base/source.h"
#include "../utils/ffmpegfactory.h"


/// @brief Image source that loads and renders static image files
class ImageSource : public Source, protected QOpenGLFunctions {
public:
    /// @brief Constructor with image file path
    /// @param file_path Path to the image file
    explicit ImageSource(const QString &file_path);

    ~ImageSource() override;

    /// @brief Load OpenGL resources (create texture from image)
    void load_resources() override;

    /// @brief Unload OpenGL resources (delete texture)
    void unload_resources() override;

    /// @brief Render the image as an OpenGL textured quad
    void render() override;

    const char *source_type_name() const override { return "Image"; }

    const QString &file_path() const { return m_file_path; }

private:
    /// @brief Load image dimensions without creating texture
    /// @return True if image loaded successfully
    bool load_image();
    /// @brief Ensure OpenGL texture is created from image data
    void ensure_texture();

private:
    QString m_file_path;          ///< Path to the image file
    GLuint m_texture_id = 0;      ///< OpenGL texture ID
    int m_tex_width = 0;          ///< Texture width in pixels
    int m_tex_height = 0;         ///< Texture height in pixels
    bool m_gl_initialized = false; ///< Whether OpenGL functions have been initialized
};

#endif