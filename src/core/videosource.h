#ifndef OBSIM_VIDEOSOURCE_H
#define OBSIM_VIDEOSOURCE_H

#include <QOpenGLFunctions>

#include <GL/gl.h>

#include "base/source.h"
#include "base/videocaptor.h"

class OpenCVFilter;

/// @brief Video source wrapper that captures frames from a video captor and renders to OpenGL texture
class VideoSource : public Source {
public:
    /// @brief Constructor with a video captor
    /// @param captor Unique pointer to video captor
    explicit VideoSource(std::unique_ptr<VideoCaptor> captor);
    ~VideoSource() override = default;

    /// @brief Load OpenGL resources
    void load_resources() override;
    /// @brief Unload OpenGL resources
    void unload_resources() override;
    /// @brief Render the video frame
    void render() override;
    /// @brief Update the video frame from captor
    /// @return True if frame was updated, false otherwise
    bool update_frame() override;
    /// @brief Set callback for frame ready notification
    /// @param callback Frame ready callback function
    void set_frame_ready_callback(VideoCaptor::FrameReadyCallback callback);
    /// @brief Stop video capture
    void stop_capture();
    /// @brief Pause video capture
    void pause_capture();
    /// @brief Resume video capture
    void resume_capture();
    /// @brief Get pointer to OpenCV filter
    /// @return Pointer to OpenCVFilter if filter is enabled, nullptr otherwise
    OpenCVFilter* filter();
    /// @brief Update filtered frame from filter queue
    /// @return True if filtered frame was updated, false otherwise
    bool update_filtered_frame();
    /// @brief Start filter processing in separate thread
    void start_filter();
    /// @brief Stop filter processing
    void stop_filter();

protected:
    /// @brief Create OpenGL texture with given dimensions
    /// @param width Texture width
    /// @param height Texture height
    void create_texture(int width, int height);
    /// @brief Upload AVFrame data to OpenGL texture
    /// @param frame Pointer to AVFrame
    void upload_frame_to_texture(const AVFramePtr& frame);

    std::unique_ptr<VideoCaptor> m_capture;               ///< Video captor instance
    GLuint m_texture_id = 0;                             ///< OpenGL texture ID
    int    m_tex_width = 0;                              ///< Texture width in pixels
    int    m_tex_height = 0;                             ///< Texture height in pixels
    bool   m_texture_created = false;                     ///< Whether texture has been created
};

#endif
