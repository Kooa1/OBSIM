#ifndef OBSIM_SCENEPREVIEWWIDGET_H
#define OBSIM_SCENEPREVIEWWIDGET_H

#include <QDebug>
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QMouseEvent>
#include <QPointer>
#include <QTimer>
#include <functional>
#include <memory>
#include <vector>
#include <algorithm>
#include <chrono>

#include "../../core/base/source.h"
#include "../../core/scene.h"
#include "../test/testsource.h"
#include "../../core/base/videocaptor.h"
#include "../../core/screencapturesource.h"
#include "../../core/cameracapturesource.h"
#include "../../core/textsource.h"
#include "../../core/imagesource.h"
#include "../../utils/devicemanager.h"

/// @brief Data structure holding a scene and its owned sources
struct SceneData {
    Scene scene;                                       ///< Scene object
    std::vector<std::unique_ptr<Source>> source_storage; ///< Owning storage for sources
    QString name;                                      ///< Scene name
};

/// @brief OpenGL widget for rendering and interacting with scene sources
class ScenePreviewWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit ScenePreviewWidget(QWidget *parent = nullptr);

    ~ScenePreviewWidget() override;

    /// @brief Add a test source for debugging
    void add_test_source();

    /// @brief Add a screen capture source
    /// @param config Capture configuration
    /// @param name Source display name
    void add_screen_capture_source(const CaptorConfig &config, const QString &name = QString());

    /// @brief Add a camera capture source
    /// @param name Source display name
    /// @param device_description Device description string
    void add_camera_capture_source(const QString &name, std::string device_description = "");

    /// @brief Add a text source
    /// @param text Text content
    /// @param font Text font
    /// @param color Text color
    /// @param name Source display name
    void add_text_source(const QString &text, const QFont &font, const QColor &color, const QString &name = QString());

    /// @brief Add an image source
    /// @param file_path Image file path
    /// @param name Source display name
    void add_image_source(const QString &file_path, const QString &name = QString());

    /// @brief Remove a source by index
    /// @param index Source index
    void remove_source(int index);

    /// @brief Select a source by index
    /// @param index Source index
    void select_source_at(int index);

    /// @brief Move a source up (change render order)
    /// @param index Source index
    void move_source_up(int index);
    /// @brief Move a source down (change render order)
    /// @param index Source index
    void move_source_down(int index);

    /// @brief Add a new scene
    /// @param name Scene name
    /// @return New scene index
    int add_scene(const QString &name);
    /// @brief Remove a scene by index
    /// @param index Scene index
    void remove_scene(int index);
    /// @brief Switch to scene at index
    /// @param index Scene index
    void switch_to_scene(int index);
    /// @brief Get the number of scenes
    /// @return Scene count
    int scene_count() const { return static_cast<int>(m_scene_list.size()); }
    /// @brief Get scene name at index
    /// @param index Scene index
    /// @return Scene name
    QString scene_name_at(int index) const;
    /// @brief Get current scene index
    /// @return Current scene index
    int current_scene_index() const { return m_current_scene_index; }

    /// @brief Clear all scenes (does not add a default scene)
    void clear_all_scenes();

    /// @brief Pause all video sources in non-current scenes
    void pause_all_non_current_scenes();

    /// @brief Get read-only access to all scene data
    /// @return Const reference to scene list
    const std::vector<SceneData>& all_scenes() const { return m_scene_list; }

    /// @brief Type definition for frame capture callback (data: BGRA, stride: width*4)
    using FrameCaptureCallback = std::function<void(const uint8_t *data, int stride, int w, int h)>;
    /// @brief Set frame capture callback for recording
    /// @param cb Callback function
    void set_frame_capture_callback(FrameCaptureCallback cb) { m_frame_capture_callback = std::move(cb); }

    static constexpr float CANVAS_W = 1920.0f; ///< Canvas width
    static constexpr float CANVAS_H = 1080.0f; ///< Canvas height

    /// @brief Get source count in current scene
    /// @return Source count
    size_t source_count() const;
    /// @brief Get source at index from current scene
    /// @param index Source index
    /// @return Pointer to source
    Source* source_at(size_t index) const;

signals:
    /// @brief Emitted when canvas selection changes
    void canvas_selection_changed(int index);
    /// @brief Emitted when a source position changes
    void source_position_changed();

private:
    void rendering_view();
    void setup_viewport_and_clear();
    void render_canvas_background();
    void render_source_stencil_for_selected();
    void render_all_sources_with_clip();
    void render_mosaic_for_selected();
    void update_all_video_sources();

    QPointF screen_to_canvas(const QPointF &screen_pos) const;
    void update_cursor();
    void ensure_mosaic_list(int w, int h);
    void delete_mosaic_list();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int m_viewX = 0;           ///< Viewport X offset
    int m_viewY = 0;           ///< Viewport Y offset
    int m_viewW = 1920;        ///< Viewport width
    int m_viewH = 1080;        ///< Viewport height

    GLuint m_mosaic_list = 0;  ///< Mosaic display list
    int m_mosaic_w = 0;        ///< Mosaic cached width
    int m_mosaic_h = 0;        ///< Mosaic cached height

    std::vector<SceneData> m_scene_list;  ///< All scenes
    int m_current_scene_index = 0;        ///< Current scene index

    Scene &current_scene() { return m_scene_list[m_current_scene_index].scene; }
    const Scene &current_scene() const { return m_scene_list[m_current_scene_index].scene; }
    std::vector<std::unique_ptr<Source>> &current_storage() {
        return m_scene_list[m_current_scene_index].source_storage;
    }
    const std::vector<std::unique_ptr<Source>> &current_storage() const {
        return m_scene_list[m_current_scene_index].source_storage;
    }

    QPointer<ScenePreviewWidget> m_self_guard;     ///< Thread-safe self reference
    QTimer *m_render_timer = nullptr;              ///< 60fps render timer

    FrameCaptureCallback m_frame_capture_callback;                    ///< Frame capture callback
    std::chrono::steady_clock::time_point m_last_capture_time;        ///< Last capture timestamp
    int m_capture_fps = 30;                                           ///< Capture frame rate
};

#endif